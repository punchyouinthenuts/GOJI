#include "goji.h"
#include "ui_GOJI.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

bool Goji::jobExists(const QString& year, const QString& month, const QString& week)
{
    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM jobs WHERE year = :year AND month = :month AND week = :week");
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":week", week);
    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }
    return false;
}

void Goji::insertJob()
{
    QSqlQuery query;
    query.prepare("INSERT INTO jobs (year, month, week, cbc_job_number, ncwo_job_number, inactive_job_number, prepif_job_number, exc_job_number, "
                  "cbc2_postage, cbc3_postage, exc_postage, inactive_po_postage, inactive_pu_postage, ncwo1_a_postage, ncwo1_ap_postage, "
                  "ncwo2_a_postage, ncwo2_ap_postage, prepif_postage, progress) "
                  "VALUES (:year, :month, :week, :cbc, :ncwo, :inactive, :prepif, :exc, :cbc2, :cbc3, :exc_p, :in_po, :in_pu, :nc1a, :nc1ap, :nc2a, :nc2ap, :prepif, :progress)");
    query.bindValue(":year", ui->yearDDbox->currentText());
    query.bindValue(":month", ui->monthDDbox->currentText());
    query.bindValue(":week", ui->weekDDbox->currentText());
    query.bindValue(":cbc", ui->cbcJobNumber->text());
    query.bindValue(":ncwo", ui->ncwoJobNumber->text());
    query.bindValue(":inactive", ui->inactiveJobNumber->text());
    query.bindValue(":prepif", ui->prepifJobNumber->text());
    query.bindValue(":exc", ui->excJobNumber->text());
    query.bindValue(":cbc2", ui->cbc2Postage->text());
    query.bindValue(":cbc3", ui->cbc3Postage->text());
    query.bindValue(":exc_p", ui->excPostage->text());
    query.bindValue(":in_po", ui->inactivePOPostage->text());
    query.bindValue(":in_pu", ui->inactivePUPostage->text());
    query.bindValue(":nc1a", ui->ncwo1APostage->text());
    query.bindValue(":nc1ap", ui->ncwo1APostage->text());
    query.bindValue(":nc2a", ui->ncwo2APostage->text());
    query.bindValue(":nc2ap", ui->ncwo2APostage->text());
    query.bindValue(":prepif", ui->prepifPostage->text());
    query.bindValue(":progress", "created");
    if (!query.exec()) {
        qDebug() << "Insert error:" << query.lastError().text();
    }
}

void Goji::updateJob()
{
    QSqlQuery query;
    query.prepare("UPDATE jobs SET cbc_job_number = :cbc, ncwo_job_number = :ncwo, inactive_job_number = :inactive, prepif_job_number = :prepif, exc_job_number = :exc, "
                  "cbc2_postage = :cbc2, cbc3_postage = :cbc3, exc_postage = :exc_p, inactive_po_postage = :in_po, inactive_pu_postage = :in_pu, "
                  "ncwo1_a_postage = :nc1a, ncwo1_ap_postage = :nc1ap, ncwo2_a_postage = :nc2a, ncwo2_ap_postage = :nc2ap, prepif_postage = :prepif, "
                  "progress = :progress "
                  "WHERE year = :year AND month = :month AND week = :week");
    query.bindValue(":cbc", ui->cbcJobNumber->text());
    query.bindValue(":ncwo", ui->ncwoJobNumber->text());
    query.bindValue(":inactive", ui->inactiveJobNumber->text());
    query.bindValue(":prepif", ui->prepifJobNumber->text());
    query.bindValue(":exc", ui->excJobNumber->text());
    query.bindValue(":cbc2", ui->cbc2Postage->text());
    query.bindValue(":cbc3", ui->cbc3Postage->text());
    query.bindValue(":exc_p", ui->excPostage->text());
    query.bindValue(":in_po", ui->inactivePOPostage->text());
    query.bindValue(":in_pu", ui->inactivePUPostage->text());
    query.bindValue(":nc1a", ui->ncwo1APostage->text());
    query.bindValue(":nc1ap", ui->ncwo1APostage->text());
    query.bindValue(":nc2a", ui->ncwo2APostage->text());
    query.bindValue(":nc2ap", ui->ncwo2APostage->text());
    query.bindValue(":prepif", ui->prepifPostage->text());
    query.bindValue(":progress", "updated");
    query.bindValue(":year", originalYear);
    query.bindValue(":month", originalMonth);
    query.bindValue(":week", originalWeek);
    if (!query.exec()) {
        qDebug() << "Update error:" << query.lastError().text();
    }
}

void Goji::deleteJob(const QString& year, const QString& month, const QString& week)
{
    QSqlQuery query;
    query.prepare("DELETE FROM jobs WHERE year = :year AND month = :month AND week = :week");
    query.bindValue(":year", year);
    query.bindValue(":month", month);
    query.bindValue(":week", week);
    if (!query.exec()) {
        qDebug() << "Delete error:" << query.lastError().text();
    }
}
