#pragma once

#include <QObject>

class MojangVersionListTest : public QObject {
    Q_OBJECT
   private slots:
    void test_parseOfficialManifest();
    void test_rejectsDuplicateManifestVersions();
};
