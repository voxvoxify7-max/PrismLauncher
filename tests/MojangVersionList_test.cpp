#include <QTest>

#include "meta/MojangVersionList.h"
#include "MojangVersionList_test.h"

void MojangVersionListTest::test_parseOfficialManifest()
{
    const QByteArray manifest = R"({
        "latest": { "release": "1.21.1", "snapshot": "24w33a" },
        "versions": [
            { "id": "24w33a", "type": "snapshot", "url": "https://piston-meta.mojang.com/v1/snapshot.json", "releaseTime": "2024-08-13T12:00:00+00:00" },
            { "id": "1.21.1", "type": "release", "url": "https://piston-meta.mojang.com/v1/release.json", "releaseTime": "2024-08-08T12:00:00+00:00" },
            { "id": "b1.7.3", "type": "old_beta", "url": "https://piston-meta.mojang.com/v1/beta.json", "releaseTime": "2011-07-08T12:00:00+00:00" }
        ]
    })";

    auto parsed = Meta::MojangVersionList::parseVersionManifest(manifest);
    QVERIFY2(parsed.has_value(), qPrintable(parsed.error()));
    QCOMPARE(parsed->size(), 3);
    QCOMPARE(parsed->at(0)->version(), QString("24w33a"));
    QCOMPARE(parsed->at(0)->type(), QString("(snapshot)"));
    QCOMPARE(parsed->at(1)->version(), QString("1.21.1"));
    QCOMPARE(parsed->at(1)->type(), QString("(release)"));
    QCOMPARE(parsed->at(2)->type(), QString("(beta)"));
    QVERIFY(parsed->at(1)->isRecommended());
}

void MojangVersionListTest::test_rejectsDuplicateManifestVersions()
{
    const QByteArray manifest = R"({
        "latest": { "release": "1.21.1" },
        "versions": [
            { "id": "1.21.1", "type": "release", "url": "https://piston-meta.mojang.com/a", "releaseTime": "2024-08-08T12:00:00+00:00" },
            { "id": "1.21.1", "type": "release", "url": "https://piston-meta.mojang.com/b", "releaseTime": "2024-08-08T12:00:00+00:00" }
        ]
    })";
    QVERIFY(!Meta::MojangVersionList::parseVersionManifest(manifest).has_value());
}

QTEST_GUILESS_MAIN(MojangVersionListTest)
