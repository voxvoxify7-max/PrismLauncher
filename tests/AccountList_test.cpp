#include <QByteArray>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include "minecraft/auth/AccountList.h"

class AccountListTest : public QObject {
    Q_OBJECT
   private slots:
    void test_windows_dpapi_migrates_legacy_account_tokens()
    {
#ifndef Q_OS_WIN
        QSKIP("Windows DPAPI is only available on Windows.");
#else
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString filePath = tempDir.filePath("accounts.json");

        auto account = MinecraftAccount::createOffline("ZyronTest");
        account->accountData()->yggdrasilToken.token = "dummy-access-token";
        account->accountData()->yggdrasilToken.refresh_token = "dummy-refresh-token";

        QJsonObject legacyRoot;
        legacyRoot.insert("formatVersion", 3);
        legacyRoot.insert("accounts", QJsonArray{ account->saveToJson() });
        const QByteArray legacyData = QJsonDocument(legacyRoot).toJson();
        QFile legacyFile(filePath);
        QVERIFY(legacyFile.open(QIODevice::WriteOnly));
        QCOMPARE(legacyFile.write(legacyData), qint64(legacyData.size()));
        legacyFile.close();

        AccountList accounts;
        accounts.setListFilePath(filePath);
        QVERIFY(accounts.loadList());
        QCOMPARE(accounts.count(), 1);
        QCOMPARE(accounts.at(0)->accountData()->yggdrasilToken.token, QString("dummy-access-token"));
        QCOMPARE(accounts.at(0)->accountData()->yggdrasilToken.refresh_token, QString("dummy-refresh-token"));

        QFile migratedFile(filePath);
        QVERIFY(migratedFile.open(QIODevice::ReadOnly));
        const QByteArray migratedData = migratedFile.readAll();
        QVERIFY(!migratedData.contains("dummy-access-token"));
        QVERIFY(!migratedData.contains("dummy-refresh-token"));

        const auto migratedRoot = QJsonDocument::fromJson(migratedData).object();
        QCOMPARE(migratedRoot.value("tokenStorage").toString(), QString("windows-dpapi-v1"));
        const auto encryptedAccount = migratedRoot.value("accounts").toArray().at(0).toObject();
        QVERIFY(!encryptedAccount.value("windowsDpapi").toString().isEmpty());

        AccountList reloadedAccounts;
        reloadedAccounts.setListFilePath(filePath);
        QVERIFY(reloadedAccounts.loadList());
        QCOMPARE(reloadedAccounts.count(), 1);
        QCOMPARE(reloadedAccounts.at(0)->accountData()->yggdrasilToken.token, QString("dummy-access-token"));
#endif
    }

    void test_windows_dpapi_rejects_invalid_ciphertext()
    {
#ifndef Q_OS_WIN
        QSKIP("Windows DPAPI is only available on Windows.");
#else
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString filePath = tempDir.filePath("accounts.json");

        QJsonObject encryptedAccount;
        encryptedAccount.insert("windowsDpapi", QString::fromLatin1(QByteArray("not-encrypted").toBase64()));
        QJsonObject root;
        root.insert("formatVersion", 3);
        root.insert("tokenStorage", "windows-dpapi-v1");
        root.insert("accounts", QJsonArray{ encryptedAccount });

        const QByteArray encryptedData = QJsonDocument(root).toJson();
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(encryptedData), qint64(encryptedData.size()));
        file.close();

        AccountList accounts;
        accounts.setListFilePath(filePath);
        QVERIFY(!accounts.loadList());
        QCOMPARE(accounts.count(), 0);
#endif
    }
};

QTEST_GUILESS_MAIN(AccountListTest)

#include "AccountList_test.moc"
