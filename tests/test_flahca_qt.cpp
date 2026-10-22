// Isolated widget/controller check: never constructs the production MainWindow.
#include "ui_GOJI.h"
#include "tmflahcacontroller.h"
#include "tmflahcadbmanager.h"
#include "databasemanager.h"
#include "tmflahcaemaildialog.h"
#include <QApplication>
#include <QFile>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTextStream>
#include <QDir>
#include <QFontDatabase>
#include <QClipboard>
#include <QElapsedTimer>
#include <QThread>
#include <functional>
#include <cstdlib>

static int checks = 0;
static void check(bool condition, const char* description)
{
    if (!condition) {
        QTextStream(stderr) << "FAIL: " << description << Qt::endl;
        std::exit(1);
    }
    ++checks;
    QTextStream(stdout) << "PASS: " << description << Qt::endl;
}

static void writeFixture(const QString& path, const QByteArray& content)
{
    QFile file(path);
    check(file.open(QIODevice::WriteOnly), "Fixture file can be created");
    check(file.write(content) == content.size(), "Fixture bytes written");
}

static bool waitFor(const std::function<bool()>& ready)
{
    QElapsedTimer timer;
    timer.start();
    while (!ready() && timer.elapsed() < 10000) {
        QApplication::processEvents();
        QThread::msleep(10);
    }
    return ready();
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    // The Windows offscreen platform needs fonts loaded explicitly for previews.
    for (const QString& fontDir : {QString("C:/Windows/Fonts"),
                                  QDir::homePath() + "/AppData/Local/Microsoft/Windows/Fonts"}) {
        const QDir fonts(fontDir);
        const QStringList names = fonts.entryList({"arial*.ttf", "consola*.ttf", "BlenderPro-*.ttf", "Iosevka*.ttf"}, QDir::Files);
        for (const QString& name : names) QFontDatabase::addApplicationFont(fonts.filePath(name));
    }
    QTemporaryDir temp;
    check(temp.isValid(), "Temporary workspace available");
    const QString base = temp.path() + "/FL AHCA";
    check(DatabaseManager::instance()->initialize(temp.path() + "/goji.db"), "Disposable database initialized");
    QSqlQuery query(DatabaseManager::instance()->getDatabase());
    check(query.exec("CREATE TABLE tm_fler_jobs (job_number TEXT)"), "FL ER sentinel table created");
    check(query.exec("INSERT INTO tm_fler_jobs VALUES ('99999')"), "FL ER sentinel retained for comparison");

    QMainWindow window;
    Ui::MainWindow ui;
    ui.setupUi(&window);
    TMFLAHCAController controller(&window, base);
    controller.setJobNumberBox(ui.jobNumberBoxTMFLAHCA);
    controller.setYearDropdown(ui.yearDDboxTMFLAHCA);
    controller.setMonthDropdown(ui.monthDDboxTMFLAHCA);
    controller.setPostageBox(ui.postageBoxTMFLAHCA);
    controller.setCountBox(ui.countBoxTMFLAHCA);
    controller.setJobDataLockButton(ui.lockButtonTMFLAHCA);
    controller.setEditButton(ui.editButtonTMFLAHCA);
    controller.setPostageLockButton(ui.postageLockTMFLAHCA);
    controller.setOpenBulkMailerButton(ui.openBulkMailerTMFLAHCA);
    controller.setRunInitialButton(ui.runInitialTMFLAHCA);
    controller.setFinalStepButton(ui.finalStepTMFLAHCA);
    controller.setTerminalWindow(ui.terminalWindowTMFLAHCA);
    controller.setTextBrowser(ui.textBrowserTMFLAHCA);
    controller.setTracker(ui.trackerTMFLAHCA);
    controller.setDropWindow(ui.dropWindowTMFLAHCA);

    check(ui.tabWidget->widget(ui.tabWidget->count() - 1) == ui.TMFLAHCA, "AHCA is the last TRACHMAR tab");
    check(ui.tabWidget->tabText(ui.tabWidget->count() - 1) == "FL AHCA", "Last tab title is FL AHCA");
    check(!window.findChild<QWidget*>("TMTARRAGON"), "Retired tab is absent");
    check(ui.logLabelTMFLAHCA->text() == "FL AHCA LOG", "AHCA log label text");
    check(ui.logLabelTMFLAHCA->geometry().center().x() == ui.trackerTMFLAHCA->geometry().center().x(), "Log label is centered over tracker");
    check(ui.dropWindowTMFLAHCA->getTargetDirectory() == base + "/RAW INPUT", "File drop targets isolated AHCA RAW INPUT");
    check(ui.textBrowserTMFLAHCA->toPlainText().contains("FL AHCA"), "Getting Started HTML loaded");
    check(!ui.textBrowserTMFLAHCA->toPlainText().contains("FL ER"), "Getting Started HTML has no old job label");
    check(!ui.runInitialTMFLAHCA->isEnabled() && !ui.finalStepTMFLAHCA->isEnabled(), "Processing buttons initially locked");

    ui.jobNumberBoxTMFLAHCA->setText("12345");
    ui.yearDDboxTMFLAHCA->setCurrentText("2026");
    ui.monthDDboxTMFLAHCA->setCurrentText("10");
    ui.lockButtonTMFLAHCA->click();
    check(controller.isJobDataLocked(), "Job lock opens monthly AHCA job");
    check(ui.runInitialTMFLAHCA->isEnabled(), "Initial button enabled after job lock");
    check(!ui.dropWindowTMFLAHCA->isEnabled(), "File drop locked during an open job");
    check(ui.textBrowserTMFLAHCA->toPlainText().contains("GOJI TM FL AHCA 01"), "Processing HTML uses AHCA TaskMaster names");
    check(!ui.textBrowserTMFLAHCA->toPlainText().contains("FL ER"), "Processing HTML has no old job label");
    ui.postageBoxTMFLAHCA->setText("12.34");
    ui.countBoxTMFLAHCA->setText("100");
    ui.postageLockTMFLAHCA->click();
    check(controller.isPostageDataLocked(), "Postage lock persists AHCA postage");
    check(ui.finalStepTMFLAHCA->isEnabled(), "Final button enabled after postage lock");
    check(query.exec("SELECT description FROM tm_flahca_log WHERE job_number='12345'") && query.next()
          && query.value(0).toString().contains("FL AHCA"), "Tracker row stored in independent AHCA table");
    check(query.exec("SELECT job_number FROM tm_fler_jobs") && query.next()
          && query.value(0).toString() == "99999", "Existing FL ER records unchanged");

    // Exercise the real asynchronous runner and handoff against harmless fixture scripts.
    check(QDir().mkpath(base + "/scripts"), "Isolated script fixture directory created");
    writeFixture(base + "/DATA/Fixture_MERGED.csv", "MAILED\n13\n");
    writeFixture(base + "/scripts/02 FINAL PROCESS.py", QByteArray(
        "import sys\nfrom pathlib import Path\n"
        "root=Path(__file__).parent\n"
        "with (root/'calls.txt').open('a') as f: f.write(' '.join(sys.argv[1:])+'\\n')\n"
        "if sys.argv[-1]=='prearchive':\n"
        " print('=== NAS_FOLDER_PATH ===')\n"
        " print(root.parent/'DATA')\n"
        " print('=== END_NAS_FOLDER_PATH ===')\n"
        " print('=== PRINT_FOLDER_PATH ===')\n"
        " print(root.parent/'NETWORK')\n"
        " print('=== END_PRINT_FOLDER_PATH ===')\n"
        " print('=== PAUSE_FOR_EMAIL ===',flush=True)\n"));
    ui.finalStepTMFLAHCA->click();
    check(controller.isProcessing(), "Script execution immediately marks AHCA busy");
    check(!controller.autoSaveAndCloseCurrentJob(), "An executing step prevents close");
    check(waitFor([&]() { return window.findChild<TMFLAHCAEmailDialog*>() != nullptr; }), "Successful preparation opens email handoff after process exit");
    check(!ui.finalStepTMFLAHCA->isEnabled(), "Final button disabled during email handoff");
    auto* cancelled = window.findChild<TMFLAHCAEmailDialog*>();
    auto* printPath = cancelled->findChild<QLineEdit*>("printPathTMFLAHCA");
    auto* copyPrint = cancelled->findChild<QPushButton*>("copyPrintPathTMFLAHCA");
    check(printPath && QDir::fromNativeSeparators(printPath->text()) == base + "/NETWORK", "Popup shows separately captured print destination");
    check(copyPrint && copyPrint->isEnabled(), "Print-path copy button enabled for delivery");
    copyPrint->click();
    check(QApplication::clipboard()->text() == printPath->text(), "COPY places exact print folder path on clipboard");
    auto* attachments = cancelled->findChild<TMFLAHCAEmailFileListWidget*>();
    check(attachments && attachments->count() == 1
          && attachments->item(0)->data(Qt::UserRole).toString() == base + "/DATA/Fixture_MERGED.csv",
          "Merged attachment remains in local DATA independently of server path");
    if (argc > 1) {
        const QString popupPreview = QFileInfo(QString::fromLocal8Bit(argv[1])).dir().filePath("fl-ahca-email-preview.png");
        check(cancelled->grab().save(popupPreview), "Email popup preview saved");
    }
    cancelled->reject();
    check(waitFor([&]() { return !controller.isProcessing(); }), "Cancelling handoff clears busy state");
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QFile calls(base + "/scripts/calls.txt");
    check(calls.open(QIODevice::ReadOnly) && calls.readAll().count('\n') == 1, "Cancelling email does not start archive");
    calls.close();
    ui.finalStepTMFLAHCA->click();
    check(waitFor([&]() { return window.findChild<TMFLAHCAEmailDialog*>() != nullptr; }), "Preparation can be retried after cancellation");
    window.findChild<TMFLAHCAEmailDialog*>()->accept();
    check(waitFor([&]() { return !controller.isProcessing(); }), "Accepting handoff completes separate archive invocation");
    check(calls.open(QIODevice::ReadOnly), "Script invocation log readable");
    const QByteArray invocations = calls.readAll();
    calls.close();
    check(invocations.count("12345 2026 10 --mode prearchive") == 2
          && invocations.count("12345 2026 10 --mode archive") == 1,
          "Correct job identity and stage arguments used without duplicate archive");

    writeFixture(base + "/DATA/TMFLAHCA SOURCE.csv", "source mapping\n");
    writeFixture(base + "/RAW INPUT/original.xlsx", "original bytes\n");
    check(controller.autoSaveAndCloseCurrentJob(), "Job closes successfully");
    check(QDir(base + "/DATA").isEmpty() && QDir(base + "/RAW INPUT").isEmpty(), "Working folders cleared by safe move on close");
    check(controller.loadJob("12345", "2026", "10"), "Saved monthly job reopens");
    check(QFile::exists(base + "/DATA/TMFLAHCA SOURCE.csv") && QFile::exists(base + "/RAW INPUT/original.xlsx"), "Mapping and original restored together");
    check(controller.isPostageDataLocked(), "Postage lock restored after reopen");
    check(ui.postageBoxTMFLAHCA->text().contains("12.34") && ui.countBoxTMFLAHCA->text() == "100", "Saved postage and count restored");

    // A collision must preserve both working sets and leave the job open.
    const QString stash = base + "/ARCHIVE/12345 OCT 2026/WORKING/DATA";
    check(QDir().mkpath(stash), "Collision fixture directory created");
    writeFixture(stash + "/existing.csv", "do not overwrite\n");
    check(!controller.autoSaveAndCloseCurrentJob(), "Close refuses existing archive working files");
    check(controller.isJobDataLocked() && QFile::exists(base + "/RAW INPUT/original.xlsx"), "Failed close retains open job and original data");
    check(QFile::remove(stash + "/existing.csv"), "Only collision fixture removed");

    if (argc > 1) {
        ui.terminalWindowTMFLAHCA->clear();
        controller.outputToTerminal("FL AHCA interface verified using isolated test data.", BaseTrackerController::Success);
        ui.tabWidget->setCurrentWidget(ui.TMFLAHCA);
        for (QTabWidget* tabs : window.findChildren<QTabWidget*>()) {
            if (tabs->indexOf(ui.TRACHMAR) >= 0) tabs->setCurrentWidget(ui.TRACHMAR);
        }
        window.resize(1900, 1100);
        window.show();
        QApplication::processEvents();
        check(window.grab().save(QString::fromLocal8Bit(argv[1])), "AHCA interface preview saved");
    }
    check(controller.autoSaveAndCloseCurrentJob(), "Final isolated job closes");
    ui.jobNumberBoxTMFLAHCA->setText("54321");
    ui.monthDDboxTMFLAHCA->setCurrentText("11");
    ui.lockButtonTMFLAHCA->click();
    check(controller.isJobDataLocked(), "Next month's job opens");
    check(query.exec("SELECT COUNT(*) FROM tm_flahca_jobs") && query.next()
          && query.value(0).toInt() == 2, "Starting next month preserves the prior saved job");
    check(controller.autoSaveAndCloseCurrentJob(), "Next month's job closes");
    check(controller.loadJob("12345", "2026", "10"), "Prior month remains available after next month's job");
    check(controller.autoSaveAndCloseCurrentJob(), "Prior month closes after regression check");
    TMFLAHCAEmailDialog noPrint("12345", nullptr, base + "/DATA");
    check(!noPrint.findChild<QPushButton*>("copyPrintPathTMFLAHCA")->isEnabled(), "No-print handoff disables path copy");
    QTextStream(stdout) << checks << " checks passed" << Qt::endl;
    return 0;
}
