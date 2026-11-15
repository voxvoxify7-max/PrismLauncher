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

#pragma once

#include "BaseVersionList.h"
#include "Result.h"
#include "Version.h"

#include <QByteArray>
#include <QList>

namespace Meta {

class MojangVersionList final : public BaseVersionList {
   public:
    using BaseVersionList::BaseVersionList;

    Task::Ptr getLoadTask(bool forceReload = false) override;
    bool isLoaded() const override { return m_loaded; }
    const BaseVersion::Ptr at(int i) const override;
    int count() const override { return m_versions.size(); }
    void sortVersions() override;
    BaseVersion::Ptr getRecommended() const override;
    QVariant data(const QModelIndex& index, int role) const override;
    RoleList providesRoles() const override;

    static Result<QList<Version::Ptr>> parseVersionManifest(const QByteArray& data);

    void setVersions(QList<Version::Ptr> versions);

   protected slots:
    void updateListData(QList<BaseVersion::Ptr> versions) override;

   private:
    QList<Version::Ptr> m_versions;
    Version::Ptr m_recommended;
    bool m_loaded = false;
};

}  // namespace Meta
