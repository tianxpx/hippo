#include "searchindex.h"
#include "note.h"
#include "Logger.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

SearchIndex::SearchIndex(QObject *parent) :
    QObject(parent)
{
}

void SearchIndex::buildSearchIndex() {

    LOG_DEBUG("buildSearchIndex()");

    QStringList notes = getUnindexedNotesList();

    if (notes.isEmpty())
        return;

    QString guid;
    foreach(guid, notes) {

        Note *n = Note::fromGUID(guid);

        if (n == NULL)
            continue;

        writeNoteIndex(guid, n->getTitle(), n->getContentTxt());

        delete n;
    }

}

QStringList SearchIndex::getUnindexedNotesList() {

    QStringList result;
    QSqlQuery query;

    if (!query.exec("SELECT notes.guid FROM notes LEFT JOIN noteIndexGUIDs ON notes.guid = noteIndexGUIDs.guid WHERE notes.active='true' AND (noteIndexGUIDs.docid IS NULL OR noteIndexGUIDs.docid=0)"))
        LOG_ERROR("SQL: " + query.lastError().text());

    while (query.next())
        result.append(query.value(0).toString());

    LOG_DEBUG(QString("%1 without index").arg(result.count()));

    return result;
}

void SearchIndex::writeNoteIndex(QString guid, QString title, QString content) {

    LOG_DEBUG(content);

    QSqlQuery query;

    query.prepare("INSERT INTO noteIndex(title, content) VALUES (:title, :content);");
    query.bindValue(":title", title);
    query.bindValue(":content", content);

    if (!query.exec()) {
        LOG_ERROR("SQL: " + query.lastError().text());
        return;
    }

    bool ok;
    int docid = query.lastInsertId().toInt(&ok);

    if (!ok || (docid == 0))
        return;

    query.prepare("INSERT INTO noteIndexGUIDs(docid, guid) VALUES (:docid, :guid);");
    query.bindValue(":docid", docid);
    query.bindValue(":guid", guid);
    if (!query.exec())
        LOG_ERROR("SQL: " + query.lastError().text());

}

QStringList SearchIndex::search(QString query) {

    LOG_DEBUG(query);

    QSqlQuery sql;

    sql.prepare("SELECT noteIndexGUIDs.guid FROM noteIndex LEFT JOIN noteIndexGUIDs ON noteIndexGUIDs.docid = noteIndex.docid WHERE noteIndex MATCH :query");
    sql.bindValue(":query", query);
    if (!sql.exec())
        LOG_ERROR("SQL: " + sql.lastError().text());

    QStringList guids;

    while (sql.next())
        guids.append(sql.value(0).toString());

    LOG_DEBUG(QString("found %1 notes").arg(guids.count()));

    return guids;
}

void SearchIndex::dropIndex() {
    QSqlQuery sql;

    if (!sql.exec("DELETE FROM noteIndexGUIDs;"))
        LOG_ERROR("SQL: " + sql.lastError().text());

    if (!sql.exec("DELETE FROM noteIndex;"))
        LOG_ERROR("SQL: " + sql.lastError().text());
}

void SearchIndex::dropNoteIndex(QString guid) {

    if (guid.isEmpty())
        return;

    QSqlQuery sql;

    sql.prepare("SELECT docid FROM noteIndexGUIDs WHERE guid=:guid");
    sql.bindValue(":guid", guid);

    if (!sql.exec()) {
        LOG_ERROR("SQL: " + sql.lastError().text());
        return;
    }

    if (!sql.next())
        return;

    int docid = sql.value(0).toInt();

    sql.prepare("DELETE FROM noteIndexGUIDs WHERE guid=:guid");
    sql.bindValue(":guid", guid);

    if (!sql.exec())
        LOG_ERROR("SQL: " + sql.lastError().text());

    sql.prepare("DELETE FROM noteIndex WHERE docid=:docid");
    sql.bindValue(":docid", docid);

    if (!sql.exec())
        LOG_ERROR("SQL: " + sql.lastError().text());

}

void SearchIndex::updateNoteIndex(QString guid) {

    dropNoteIndex(guid);
    buildSearchIndex();
}