/* Copyright 2015-2021 MultiMC Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "MojangVersionList.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>
#include <QUrl>
#include <algorithm>
#include <utility>

#include "Application.h"
#include "VersionList.h"
#include "net/ApiRequest.h"
#include "net/NetJob.h"

namespace Meta {
namespace {
constexpr auto MojangVersionManifestUrl = "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json";

QString typeForManifestVersion(const QString& type)
{
    if (type == "release") {
        return "(release)";
    }
    if (type == "snapshot") {
        return "(snapshot)";
    }
    if (type == "old_beta") {
        return "(beta)";
    }
    if (type == "old_alpha") {
        return "(alpha)";
    }
    return "(experiment)";
}

class MojangVersionManifestTask final : public Task {
   public:
    explicit MojangVersionManifestTask(MojangVersionList* list) : m_list(list) {}

   protected:
    void executeTask() override
    {
        setStatus(QObject::tr("Loading official Minecraft versions"));
        m_job = makeShared<NetJob>(QObject::tr("Download official Minecraft version manifest"), APPLICATION->network());
        m_job->setAskRetry(false);

        auto [request, response] = Net::ApiRequest::makeByteArray(QUrl(QString::fromLatin1(MojangVersionManifestUrl)));
        m_response = response;
        m_job->addNetAction(request);

        connect(m_job.get(), &Task::progress, this, &Task::setProgress);
        connect(m_job.get(), &Task::status, this, &Task::setStatus);
        connect(m_job.get(), &Task::details, this, &Task::setDetails);
        connect(m_job.get(), &Task::failed, this, [this](const QString& reason) { emitFailed(reason); });
        connect(m_job.get(), &Task::aborted, this, [this] { emitAborted(); });
        connect(m_job.get(), &Task::succeeded, this, [this] {
            auto versions = MojangVersionList::parseVersionManifest(*m_response);
            if (!versions) {
                emitFailed(versions.error());
                return;
            }
            m_list->setVersions(std::move(versions.value()));
            emitSucceeded();
        });
        m_job->start();
    }

   private:
    MojangVersionList* m_list;
    NetJob::Ptr m_job;
    QByteArray* m_response = nullptr;
};
}  // namespace

Task::Ptr MojangVersionList::getLoadTask([[maybe_unused]] bool forceReload)
{
    return makeShared<MojangVersionManifestTask>(this);
}

const BaseVersion::Ptr MojangVersionList::at(int i) const
{
    return m_versions.at(i);
}

void MojangVersionList::sortVersions()
{
    beginResetModel();
    std::ranges::sort(m_versions, [](const Version::Ptr& a, const Version::Ptr& b) { return a->rawTime() > b->rawTime(); });
    endResetModel();
}

BaseVersion::Ptr MojangVersionList::getRecommended() const
{
    return m_recommended;
}

QVariant MojangVersionList::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_versions.size() || index.parent().isValid()) {
        return {};
    }

    const auto& version = m_versions.at(index.row());
    switch (role) {
        case VersionPointerRole:
            return QVariant::fromValue(std::dynamic_pointer_cast<BaseVersion>(version));
        case VersionRole:
        case VersionIdRole:
            return version->descriptor();
        case TypeRole:
            return version->typeString();
        case RecommendedRole:
            return version == m_recommended;
        case Meta::VersionList::TimeRole:
            return version->time();
        case SortRole:
            return version->rawTime();
        default:
            return {};
    }
}

BaseVersionList::RoleList MojangVersionList::providesRoles() const
{
    return { VersionPointerRole, VersionRole, VersionIdRole, TypeRole, RecommendedRole, Meta::VersionList::TimeRole };
}

Result<QList<Version::Ptr>> MojangVersionList::parseVersionManifest(const QByteArray& data)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return std::unexpected(QObject::tr("Invalid Mojang version manifest: %1").arg(parseError.errorString()));
    }

    const QJsonObject manifest = document.object();
    const QJsonObject latest = manifest.value("latest").toObject();
    const QString latestRelease = latest.value("release").toString();
    const QJsonArray versionEntries = manifest.value("versions").toArray();
    if (latestRelease.isEmpty() || versionEntries.isEmpty()) {
        return std::unexpected(QObject::tr("Mojang version manifest is missing its latest release or version entries."));
    }

    QList<Version::Ptr> versions;
    versions.reserve(versionEntries.size());
    QSet<QString> seenVersions;
    Version::Ptr recommended;
    for (const QJsonValue& value : versionEntries) {
        if (!value.isObject()) {
            return std::unexpected(QObject::tr("Mojang version manifest contains an invalid version entry."));
        }

        const QJsonObject entry = value.toObject();
        const QString id = entry.value("id").toString();
        const QString type = entry.value("type").toString();
        const QString releaseTime = entry.value("releaseTime").toString();
        const QUrl versionUrl(entry.value("url").toString());
        auto date = QDateTime::fromString(releaseTime, Qt::ISODateWithMs);
        if (!date.isValid()) {
            date = QDateTime::fromString(releaseTime, Qt::ISODate);
        }
        if (id.isEmpty() || type.isEmpty() || !date.isValid() || !versionUrl.isValid() || versionUrl.scheme() != "https") {
            return std::unexpected(QObject::tr("Mojang version manifest contains an incomplete version entry."));
        }
        if (seenVersions.contains(id)) {
            return std::unexpected(QObject::tr("Mojang version manifest contains duplicate version %1.").arg(id));
        }
        seenVersions.insert(id);

        auto version = std::make_shared<Version>("net.minecraft", id);
        version->setType(typeForManifestVersion(type));
        version->setTime(date.toMSecsSinceEpoch() / 1000);
        version->setRecommended(id == latestRelease);
        if (id == latestRelease) {
            recommended = version;
        }
        versions.append(std::move(version));
    }

    if (!recommended) {
        return std::unexpected(QObject::tr("Mojang version manifest does not contain its declared latest release."));
    }
    return versions;
}

void MojangVersionList::setVersions(QList<Version::Ptr> versions)
{
    beginResetModel();
    m_versions = std::move(versions);
    std::ranges::sort(m_versions, [](const Version::Ptr& a, const Version::Ptr& b) { return a->rawTime() > b->rawTime(); });
    m_recommended.reset();
    for (const auto& version : m_versions) {
        if (version->isRecommended()) {
            m_recommended = version;
            break;
        }
    }
    m_loaded = true;
    endResetModel();
}

void MojangVersionList::updateListData(QList<BaseVersion::Ptr> versions)
{
    QList<Version::Ptr> typedVersions;
    typedVersions.reserve(versions.size());
    for (const auto& version : versions) {
        auto typedVersion = std::dynamic_pointer_cast<Version>(version);
        if (!typedVersion) {
            qCritical() << "MojangVersionList received a version from an incompatible source.";
            return;
        }
        typedVersions.append(std::move(typedVersion));
    }
    setVersions(std::move(typedVersions));
}

}  // namespace Meta
