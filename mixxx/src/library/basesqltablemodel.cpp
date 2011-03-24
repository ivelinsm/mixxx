// basesqltablemodel.h
// Created by RJ Ryan (rryan@mit.edu) 1/29/2010

#include <QtDebug>
#include <QTime>

#include "library/basesqltablemodel.h"
#include "mixxxutils.cpp"
#include "library/starrating.h"

const bool sDebug = false;

BaseSqlTableModel::BaseSqlTableModel(QObject* pParent,
                                     TrackCollection* pTrackCollection,
                                     QSqlDatabase db)
        :  QAbstractTableModel(pParent),
           m_pTrackCollection(pTrackCollection),
           m_trackDAO(m_pTrackCollection->getTrackDAO()),
           m_database(db) {
    connect(&m_trackDAO, SIGNAL(trackChanged(int)),
            this, SLOT(trackChanged(int)));
    connect(&m_trackDAO, SIGNAL(trackClean(int)),
            this, SLOT(trackClean(int)));
    m_bInitialized = false;
    m_bIndexBuilt = false;
    m_iSortColumn = 0;
    m_eSortOrder = Qt::AscendingOrder;
}

BaseSqlTableModel::~BaseSqlTableModel() {

}

void BaseSqlTableModel::initHeaderData() {
    //Set the column heading labels, rename them for translations and have
    //proper capitalization
    setHeaderData(fieldIndex(LIBRARYTABLE_TIMESPLAYED),
                  Qt::Horizontal, tr("Played"));
    setHeaderData(fieldIndex(LIBRARYTABLE_ARTIST),
                  Qt::Horizontal, tr("Artist"));
    setHeaderData(fieldIndex(LIBRARYTABLE_TITLE),
                  Qt::Horizontal, tr("Title"));
    setHeaderData(fieldIndex(LIBRARYTABLE_ALBUM),
                  Qt::Horizontal, tr("Album"));
    setHeaderData(fieldIndex(LIBRARYTABLE_GENRE),
                  Qt::Horizontal, tr("Genre"));
    setHeaderData(fieldIndex(LIBRARYTABLE_YEAR),
                  Qt::Horizontal, tr("Year"));
    setHeaderData(fieldIndex(LIBRARYTABLE_FILETYPE),
                  Qt::Horizontal, tr("Type"));
    setHeaderData(fieldIndex(LIBRARYTABLE_LOCATION),
                  Qt::Horizontal, tr("Location"));
    setHeaderData(fieldIndex(LIBRARYTABLE_COMMENT),
                  Qt::Horizontal, tr("Comment"));
    setHeaderData(fieldIndex(LIBRARYTABLE_DURATION),
                  Qt::Horizontal, tr("Duration"));
    setHeaderData(fieldIndex(LIBRARYTABLE_RATING),
                  Qt::Horizontal, tr("Rating"));
    setHeaderData(fieldIndex(LIBRARYTABLE_BITRATE),
                  Qt::Horizontal, tr("Bitrate"));
    setHeaderData(fieldIndex(LIBRARYTABLE_BPM),
                  Qt::Horizontal, tr("BPM"));
    setHeaderData(fieldIndex(LIBRARYTABLE_TRACKNUMBER),
                  Qt::Horizontal, tr("Track #"));
    setHeaderData(fieldIndex(LIBRARYTABLE_DATETIMEADDED),
                  Qt::Horizontal, tr("Date Added"));
    setHeaderData(fieldIndex(PLAYLISTTRACKSTABLE_POSITION),
                  Qt::Horizontal, tr("#"));
    setHeaderData(fieldIndex(LIBRARYTABLE_KEY),
                  Qt::Horizontal, tr("Key"));
}

void BaseSqlTableModel::initDefaultSearchColumns() {
    QStringList searchColumns;
    searchColumns << "artist"
                  << "album"
                  << "location"
                  << "comment"
                  << "title";
    setSearchColumns(searchColumns);
}

QSqlDatabase BaseSqlTableModel::database() const {
    return m_database;
}

void BaseSqlTableModel::setSearchColumns(const QStringList& searchColumns) {
    m_searchColumns = searchColumns;

    // Convert all the search column names to their field indexes because we use
    // them a bunch.
    m_searchColumnIndices.resize(m_searchColumns.size());
    for (int i = 0; i < m_searchColumns.size(); ++i) {
        m_searchColumnIndices[i] = fieldIndex(m_searchColumns[i]);
    }
}

bool BaseSqlTableModel::setHeaderData(int section, Qt::Orientation orientation,
                                      const QVariant &value, int role) {
    int numColumns = columnCount();
    if (section < 0 || section >= numColumns) {
        return false;
    }

    if (orientation != Qt::Horizontal) {
        // We only care about horizontal headers.
        return false;
    }

    if (m_headerInfo.size() != numColumns)
        m_headerInfo.resize(numColumns);

    m_headerInfo[section][role] = value;
    emit(headerDataChanged(orientation, section, section));
    return true;
}

QVariant BaseSqlTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);

    if (orientation == Qt::Horizontal) {
        QVariant headerValue = m_headerInfo.value(section).value(role);
        if (!headerValue.isValid() && role == Qt::DisplayRole) {
            // Try EditRole if DisplayRole wasn't present
            headerValue = m_headerInfo.value(section).value(Qt::EditRole);
        }
        if (!headerValue.isValid()) {
            headerValue = QVariant(section).toString();
        }
        return headerValue;
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}


void BaseSqlTableModel::updateTrackInIndex(int trackId) {
    QList<int> trackIds;
    trackIds.append(trackId);
    updateTracksInIndex(trackIds);
}

void BaseSqlTableModel::updateTracksInIndex(QList<int> trackIds) {
    if (!m_bInitialized || trackIds.size() == 0)
        return;

    QStringList idStrings;
    foreach (int trackId, trackIds) {
        idStrings << QVariant(trackId).toString();
    }

    QString queryString = QString("SELECT %1 FROM %2 WHERE %3 in (%4)")
            .arg(m_columnNamesJoined).arg(m_tableName).arg(m_idColumn).arg(idStrings.join(","));

    if (sDebug) {
        qDebug() << this << "updateTracksInIndex update query:" << queryString;
    }

    QSqlQuery query(m_database);
    // This causes a memory savings since QSqlCachedResult (what QtSQLite uses)
    // won't allocate a giant in-memory table that we won't use at all.
    query.setForwardOnly(true); // performance improvement?
    query.prepare(queryString);

    if (!query.exec()) {
        qDebug() << this << "updateTracksInIndex error:" << __FILE__ << __LINE__
                 << query.executedQuery() << query.lastError();
        return;
    }

    int idColumn = query.record().indexOf(m_idColumn);

    while (query.next()) {
        int id = query.value(idColumn).toInt();

        QVector<QVariant>& record = m_recordCache[id];
        record.resize(m_columnNames.size());

        for (int i = 0; i < m_columnNames.size(); ++i) {
            record[i] = query.value(i);
        }
    }

    foreach (int trackId, trackIds) {
        int row = getTrackRow(trackId);
        if (row >= 0) {
            //qDebug() << "Row in this result set was updated. Signalling update. track:" << trackId << "row:" << row;
            QModelIndex left = index(row, 0);
            QModelIndex right = index(row, columnCount());
            emit(dataChanged(left, right));
        }
    }
}

void BaseSqlTableModel::buildIndex() {
    if (!m_bInitialized)
        return;

    if (sDebug)
        qDebug() << this << "buildIndex()";

    QTime timer;
    timer.start();

    QString queryString = QString("SELECT %1 FROM %2").arg(m_columnNamesJoined).arg(m_tableName);

    if (sDebug)
        qDebug() << this << "buildIndex query:" << queryString;

    QSqlQuery query(m_database);
    // This causes a memory savings since QSqlCachedResult (what QtSQLite uses)
    // won't allocate a giant in-memory table that we won't use at all.
    query.setForwardOnly(true); // performance improvement?
    query.prepare(queryString);

    if (!query.exec()) {
        qDebug() << this << "buildIndex error:"
                 << __FILE__ << __LINE__ << query.executedQuery() << query.lastError();
    }

    int idColumn = query.record().indexOf(m_idColumn);

    while (query.next()) {
        int id = query.value(idColumn).toInt();

        QVector<QVariant>& record = m_recordCache[id];
        record.resize(m_columnNames.size());

        for (int i = 0; i < m_columnNames.size(); ++i) {
            record[i] = query.value(i);
        }
    }

    m_bIndexBuilt = true;
    qDebug() << this << "buildIndex took" << timer.elapsed() << "ms";
}

int BaseSqlTableModel::findSortInsertionPoint(int trackId, TrackPointer pTrack,
                                              const QVector<int>& rowToTrack) {
    QVariant trackValue = getTrackValueForColumn(trackId, m_iSortColumn, pTrack);

    int min = 0;
    int max = rowToTrack.size()-1;

    if (sDebug) {
        qDebug() << this << "Trying to insertion sort:"
                 << trackValue << "min" << min << "max" << max;
    }

    while (min <= max) {
        int mid = min + (max - min) / 2;
        int otherTrackId = rowToTrack[mid];

        // This should not happen, but it's a recoverable error so we should only log it.
        if (!m_recordCache.contains(otherTrackId)) {
            qDebug() << "WARNING: track" << otherTrackId << "was not in index";
            updateTrackInIndex(otherTrackId);
        }

        QVariant tableValue = getTrackValueForColumn(otherTrackId, m_iSortColumn);
        int compare = compareColumnValues(m_iSortColumn, m_eSortOrder, trackValue, tableValue);

        if (sDebug) {
            qDebug() << this << "Comparing" << trackValue
                     << "to" << tableValue << ":" << compare;
        }

        if (compare == 0) {
            // Alright, if we're here then we can insert it here and be
            // "correct"
            min = mid;
            break;
        } else if (compare > 0) {
            min = mid + 1;
        } else {
            max = mid - 1;
        }
    }
    return min;
}


void BaseSqlTableModel::select() {
    if (!m_bInitialized)
        return;

    if (sDebug) {
        qDebug() << this << "select()";
    }

    QTime time;
    time.start();

    // If we haven't built the index yet, now is a good time.
    if (!m_bIndexBuilt) {
        buildIndex();
    }

    // Remove all the rows from the table.
    if (m_rowToTrackId.size() > 0) {
        beginRemoveRows(QModelIndex(), 0, m_rowToTrackId.size()-1);
        m_rowToTrackId.clear();
        m_trackIdToRow.clear();
        endRemoveRows();
    }

    QString filter = filterClause();
    QString orderBy = orderByClause();
    QString queryString = QString("SELECT %1 FROM %2 %3 %4")
            .arg(m_idColumn).arg(m_tableName).arg(filter).arg(orderBy);

    if (sDebug) {
        qDebug() << this << "select() executing:" << queryString;
    }

    QSqlQuery query(m_database);
    // This causes a memory savings since QSqlCachedResult (what QtSQLite uses)
    // won't allocate a giant in-memory table that we won't use at all.
    query.setForwardOnly(true);
    query.prepare(queryString);

    if (!query.exec()) {
        qDebug() << this << "select() error:" << __FILE__ << __LINE__
                 << query.executedQuery() << query.lastError();
    }

    int idColumn = query.record().indexOf(m_idColumn);
    int rows = query.size();

    if (sDebug) {
        qDebug() << "Rows returned" << rows << m_rowToTrackId.size();
    }

    QVector<int> rowToTrack;
    QHash<int, int> trackToRow;
    QList<int> missingTracks;
    while (query.next()) {
        int id = query.value(idColumn).toInt();
        trackToRow[id] = rowToTrack.size();
        rowToTrack.push_back(id);

        if (!m_recordCache.contains(id)) {
            missingTracks.push_back(id);
        }
    }

    // TODO(XXX) This is a hack. If every part of Mixxx was being honest with us
    // whenever it updated out table, then we wouldn't have to do this. Any
    // newly added tracks that are present in this filter will need to be added
    // to the record cache.
    updateTracksInIndex(missingTracks);

    // *deep breath* Now, we have to deal with the track cache.
    //
    // Tracks that are in the track cache might be:
    //
    // 1) Updated such that they no longer should be shown for the current query
    //    when they are present in the result set.
    // 2) Updated such that they should be shown for the current query, but
    //    aren't present in the result set.
    // 3) Updated so that their sort position should be different.
    //
    // In the case of 1, we can just remove them from the results. Easy
    // enough. In the case of 2 and 3, we have to insert them into the results
    // *in the correct position, given the current column ordering*.

    // Make a regular expression that matches the query terms.
    QStringList searchTokens = m_currentSearch.split(" ");
    // Escape every token to stuff in a regular expression
    for (int i = 0; i < searchTokens.size(); ++i) {
        searchTokens[i] = QRegExp::escape(searchTokens[i]);
    }
    QRegExp searchMatcher(searchTokens.join("|"), Qt::CaseInsensitive);

    // Going off of the assumption that the track cache is really small, this
    // big dirty loop isn't so bad.
    foreach (int trackId, m_trackOverrides) {
        // Only get the track if it is in the cache.
        TrackPointer pTrack = lookupCachedTrack(trackId);

        if (!pTrack)
            continue;

        // Skip tracks that are in the override list but are not part of the
        // base "view" of this table. This requires that we get sane updates
        // from e.g. the DAO's when the data under us changes.
        if (!m_recordCache.contains(trackId))
            continue;

        // Default true
        bool shouldBeInResultSet = true;

        // Alright, now let's look at the active search query and the search
        // columns.
        if (!m_currentSearch.isEmpty()) {
            bool matches = false;

            // For every search column, lookup the value for the track and check
            // if it matches the search query.
            foreach (int columnIndex, m_searchColumnIndices) {
                QVariant value = getTrackValueForColumn(trackId, columnIndex, pTrack);
                if (value.isValid() && qVariantCanConvert<QString>(value)) {
                    QString valueStr = value.toString();
                    if (valueStr.contains(searchMatcher)) {
                        matches = true;
                    }
                }
            }

            // If we matched, then it should be in the result set. If not, then
            // it shouldn't be.
            shouldBeInResultSet = matches;
        }

        // If the track is in this result set.
        bool isInResultSet = trackToRow.contains(trackId);

        if (shouldBeInResultSet) {
            // Track should be in result set...

            // Remove the track from the results first (we have to do this or it
            // will sort wrong).
            if (isInResultSet) {
                int row = trackToRow[trackId];
                rowToTrack.remove(row);
                // Don't update trackToRow, since we do it below.
            }

            // Figure out where it is supposed to sort. The table is sorted by
            // the sort column, so we can binary search.
            int insertRow = findSortInsertionPoint(trackId, pTrack, rowToTrack);

            if (sDebug) {
                qDebug() << this << "Insertion sort says it should be inserted at:" << insertRow;
            }

            // The track should sort at insertRow
            trackToRow[trackId] = insertRow;
            rowToTrack.insert(insertRow, trackId);

            // Fix the index. TODO(rryan) find a non-stupid way to do this.
            for (int i = 0; i < rowToTrack.size(); ++i) {
                trackToRow[rowToTrack[i]] = i;

            }
        } else if (isInResultSet) {
            // Track should not be in this result set, but it is. We need to
            // remove it.
            int row = trackToRow.take(trackId);
            // It's O(n) to remove an item from a QVector. Sucks to be you.
            rowToTrack.remove(row);
        }
    }

    // We're done! Issue the update signals and replace the master maps.
    beginInsertRows(QModelIndex(), 0, rowToTrack.size()-1);
    m_rowToTrackId = rowToTrack;
    m_trackIdToRow = trackToRow;
    endInsertRows();

    int elapsed = time.elapsed();
    qDebug() << this << "select() took" << elapsed << "ms";
}

void BaseSqlTableModel::setTable(const QString& tableName,
                                 const QStringList& columnNames,
                                 const QString& idColumn) {
    qDebug() << this << "setTable" << tableName << columnNames << idColumn;
    m_tableName = tableName;
    m_columnNames = columnNames;
    m_columnNamesJoined = m_columnNames.join(",");
    m_idColumn = idColumn;
    m_columnIndex.clear();
    for (int i = 0; i < m_columnNames.size(); ++i) {
        m_columnIndex[m_columnNames[i]] = i;
    }

    m_bInitialized = true;
}

QString BaseSqlTableModel::currentSearch() const {
    return m_currentSearch;
}

void BaseSqlTableModel::search(const QString& searchText, const QString extraFilter) {
    if (sDebug)
        qDebug() << this << "search" << searchText;

    bool searchIsDifferent = m_currentSearch.isNull() || m_currentSearch != searchText;
    bool filterDisabled = (m_currentSearchFilter.isNull() && extraFilter.isNull());
    bool searchFilterIsDifferent = m_currentSearchFilter != extraFilter;

    if (!searchIsDifferent && (filterDisabled || !searchFilterIsDifferent)) {
        // Do nothing if the filters are no different.
        return;
    }

    m_currentSearch = searchText;
    m_currentSearchFilter = extraFilter;

    if (m_bIndexBuilt) {
        select();
    }
}

void BaseSqlTableModel::setSort(int column, Qt::SortOrder order) {
    if (sDebug) {
        qDebug() << this << "setSort()";
    }

    m_iSortColumn = column;
    m_eSortOrder = order;

    if (m_bIndexBuilt) {
        select();
    }
}

void BaseSqlTableModel::sort(int column, Qt::SortOrder order) {
    if (sDebug) {
        qDebug() << this << "sort()" << column << order;
    }

    m_iSortColumn = column;
    m_eSortOrder = order;

    select();
}

int BaseSqlTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_rowToTrackId.size();
}

int BaseSqlTableModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_columnNames.size();
}

int BaseSqlTableModel::fieldIndex(const QString& fieldName) const {
    // Usually a small list, so O(n) is small
    //return m_queryRecord.indexOf(fieldName);
    //return m_columnNames.indexOf(fieldName);
    QHash<QString, int>::const_iterator it = m_columnIndex.constFind(fieldName);
    if (it != m_columnIndex.end()) {
        return it.value();
    }
    return -1;
}

QVariant BaseSqlTableModel::data(const QModelIndex& index, int role) const {
    //qDebug() << this << "data()";
    if (!index.isValid() || (role != Qt::DisplayRole &&
                             role != Qt::EditRole &&
                             role != Qt::CheckStateRole &&
                             role != Qt::ToolTipRole)) {
        return QVariant();
    }

    int row = index.row();
    int column = index.column();

    // This value is the value in its most raw form. It was looked up either
    // from the SQL table or from the cached track layer.
    QVariant value = getBaseValue(index, role);

    // Format the value based on whether we are in a tooltip, display, or edit
    // role
    switch (role) {
        case Qt::ToolTipRole:
        case Qt::DisplayRole:
            if (column == fieldIndex(LIBRARYTABLE_DURATION)) {
                if (qVariantCanConvert<int>(value))
                    value = MixxxUtils::secondsToMinutes(qVariantValue<int>(value));
            } else if (column == fieldIndex(LIBRARYTABLE_RATING)) {
                if (qVariantCanConvert<int>(value))
                    value = qVariantFromValue(StarRating(value.toInt()));
            } else if (column == fieldIndex(LIBRARYTABLE_TIMESPLAYED)) {
                if (qVariantCanConvert<int>(value))
                    value =  QString("(%1)").arg(value.toInt());
            } else if (column == fieldIndex(LIBRARYTABLE_PLAYED)) {
                // Convert to a bool. Not really that useful since it gets converted
                // right back to a QVariant
                value = (value == "true") ? true : false;
            }
            break;
        case Qt::EditRole:
            if (column == fieldIndex(LIBRARYTABLE_BPM)) {
                return value.toDouble();
            } else if (column == fieldIndex(LIBRARYTABLE_TIMESPLAYED)) {
                return index.sibling(row, fieldIndex(LIBRARYTABLE_PLAYED)).data().toBool();
            } else if (column == fieldIndex(LIBRARYTABLE_RATING)) {
                if (qVariantCanConvert<int>(value))
                    value = qVariantFromValue(StarRating(value.toInt()));
            }
            break;
        case Qt::CheckStateRole:
            if (column == fieldIndex(LIBRARYTABLE_TIMESPLAYED)) {
                bool played = index.sibling(row, fieldIndex(LIBRARYTABLE_PLAYED)).data().toBool();
                value = played ? Qt::Checked : Qt::Unchecked;
            }
            break;
        default:
            break;
    }
    return value;
}

bool BaseSqlTableModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid())
        return false;

    int row = index.row();
    int column = index.column();

    if (sDebug) {
        qDebug() << this << "setData() column:" << column << "value:" << value << "role:" << role;
    }

    // Over-ride sets to TIMESPLAYED and re-direct them to PLAYED
    if (role == Qt::CheckStateRole) {
        if (column == fieldIndex(LIBRARYTABLE_TIMESPLAYED)) {
            QString val = value.toInt() > 0 ? QString("true") : QString("false");
            QModelIndex playedIndex = index.sibling(index.row(), fieldIndex(LIBRARYTABLE_PLAYED));
            return setData(playedIndex, val, Qt::EditRole);
        }
        return false;
    }

    if (row < 0 || row >= m_rowToTrackId.size()) {
        return false;
    }

    int trackId = m_rowToTrackId[row];
    TrackPointer pTrack = m_trackDAO.getTrack(trackId);
    setTrackValueForColumn(pTrack, column, value);

    // Do not save the track here. Changing the track dirties it and the caching
    // system will automatically save the track once it is unloaded from
    // memory. rryan 10/2010
    //m_trackDAO.saveTrack(pTrack);

    return true;
}

Qt::ItemFlags BaseSqlTableModel::flags(const QModelIndex &index) const {
    return readWriteFlags(index);
}

Qt::ItemFlags BaseSqlTableModel::readWriteFlags(const QModelIndex &index) const {
    if (!index.isValid())
        return Qt::ItemIsEnabled;

    Qt::ItemFlags defaultFlags = QAbstractItemModel::flags(index);

    //Enable dragging songs from this data model to elsewhere (like the waveform
    //widget to load a track into a Player).
    defaultFlags |= Qt::ItemIsDragEnabled;

    int row = index.row();
    int column = index.column();

    if ( column == fieldIndex(LIBRARYTABLE_FILETYPE)
         || column == fieldIndex(LIBRARYTABLE_LOCATION)
         || column == fieldIndex(LIBRARYTABLE_DURATION)
         || column == fieldIndex(LIBRARYTABLE_BITRATE)
         || column == fieldIndex(LIBRARYTABLE_DATETIMEADDED)) {
        return defaultFlags;
    } else if (column == fieldIndex(LIBRARYTABLE_TIMESPLAYED)) {
        return defaultFlags | Qt::ItemIsUserCheckable;
    } else {
        return defaultFlags | Qt::ItemIsEditable;
    }
}

Qt::ItemFlags BaseSqlTableModel::readOnlyFlags(const QModelIndex &index) const
{
    Qt::ItemFlags defaultFlags = QAbstractItemModel::flags(index);
    if (!index.isValid())
        return Qt::ItemIsEnabled;

    //Enable dragging songs from this data model to elsewhere (like the waveform widget to
    //load a track into a Player).
    defaultFlags |= Qt::ItemIsDragEnabled;

    return defaultFlags;
}

int BaseSqlTableModel::getTrackRow(int trackId) const {
    if (m_trackIdToRow.contains(trackId)) {
        return m_trackIdToRow[trackId];
    }
    return -1;
}

void BaseSqlTableModel::trackChanged(int trackId) {
    if (sDebug) {
        qDebug() << this << "trackChanged" << trackId;
    }
    m_trackOverrides.insert(trackId);
    int row = getTrackRow(trackId);
    if (row >= 0) {
        //qDebug() << "Row in this result set was updated. Signalling update. track:" << trackId << "row:" << row;
        QModelIndex left = index(row, 0);
        QModelIndex right = index(row, columnCount());
        emit(dataChanged(left, right));
    }
}

void BaseSqlTableModel::trackClean(int trackId) {
    if (sDebug) {
        qDebug() << this << "trackClean" << trackId;
    }
    if (m_trackOverrides.contains(trackId)) {
        m_trackOverrides.remove(trackId);
        updateTrackInIndex(trackId);
    }
}

int BaseSqlTableModel::compareColumnValues(int iColumnNumber, Qt::SortOrder eSortOrder,
                                           QVariant val1, QVariant val2) {
    int result = 0;

    if (iColumnNumber == fieldIndex(PLAYLISTTRACKSTABLE_POSITION) ||
        iColumnNumber == fieldIndex(LIBRARYTABLE_BITRATE) ||
        iColumnNumber == fieldIndex(LIBRARYTABLE_BPM) ||
        iColumnNumber == fieldIndex(LIBRARYTABLE_DURATION) ||
        iColumnNumber == fieldIndex(LIBRARYTABLE_TIMESPLAYED) ||
        iColumnNumber == fieldIndex(LIBRARYTABLE_RATING)) {
        // Sort as floats.
        double delta = val1.toDouble() - val2.toDouble();

        if (fabs(delta) < .00001)
            result = 0;
        else if (delta > 0.0)
            result = 1;
        else
            result = -1;
    } else {
        // Default to case-insensitive string comparison
        result = val1.toString().compare(val2.toString(), Qt::CaseInsensitive);
    }

    // If we're in descending order, flip the comparison.
    if (eSortOrder == Qt::DescendingOrder) {
        result = -result;
    }

    return result;
}

TrackPointer BaseSqlTableModel::lookupCachedTrack(int trackId) const {
    // Only get the Track from the TrackDAO if it's in the cache
    return m_trackDAO.getTrack(trackId, true);
}

QVariant BaseSqlTableModel::getTrackValueForColumn(int trackId, int column, TrackPointer pTrack) const {
    QVariant result;

    // The caller can optionally provide a pTrack if they already looked it
    // up. This is just an optimization to help reduce the # of calls to
    // lookupCachedTrack. If they didn't provide it, look it up.
    if (!pTrack) {
        pTrack = lookupCachedTrack(trackId);
    }
    if (pTrack) {
        result = getTrackValueForColumn(pTrack, column);
    }

    // If the track lookup failed (could happen for track properties we dont
    // keep track of in Track, like playlist position) look up the value in
    // their SQL record.
    if (!result.isValid()) {
        QHash<int, QVector<QVariant> >::const_iterator it =
                m_recordCache.find(trackId);
        if (it != m_recordCache.end()) {
            const QVector<QVariant>& fields = it.value();
            result = fields.value(column, result);
        }
    }
    return result;
}

QVariant BaseSqlTableModel::getTrackValueForColumn(TrackPointer pTrack, int column) const {
    if (!pTrack)
        return QVariant();

    // TODO(XXX) Qt properties could really help here.
    if (fieldIndex(LIBRARYTABLE_ARTIST) == column) {
        return QVariant(pTrack->getArtist());
    } else if (fieldIndex(LIBRARYTABLE_TITLE) == column) {
        return QVariant(pTrack->getTitle());
    } else if (fieldIndex(LIBRARYTABLE_ALBUM) == column) {
        return QVariant(pTrack->getAlbum());
    } else if (fieldIndex(LIBRARYTABLE_YEAR) == column) {
        return QVariant(pTrack->getYear());
    } else if (fieldIndex(LIBRARYTABLE_GENRE) == column) {
        return QVariant(pTrack->getGenre());
    } else if (fieldIndex(LIBRARYTABLE_FILETYPE) == column) {
        return QVariant(pTrack->getType());
    } else if (fieldIndex(LIBRARYTABLE_TRACKNUMBER) == column) {
        return QVariant(pTrack->getTrackNumber());
    } else if (fieldIndex(LIBRARYTABLE_LOCATION) == column) {
        return QVariant(pTrack->getLocation());
    } else if (fieldIndex(LIBRARYTABLE_COMMENT) == column) {
        return QVariant(pTrack->getComment());
    } else if (fieldIndex(LIBRARYTABLE_DURATION) == column) {
        return pTrack->getDuration();
    } else if (fieldIndex(LIBRARYTABLE_BITRATE) == column) {
        return QVariant(pTrack->getBitrate());
    } else if (fieldIndex(LIBRARYTABLE_BPM) == column) {
        return QVariant(pTrack->getBpm());
    } else if (fieldIndex(LIBRARYTABLE_PLAYED) == column) {
        return QVariant(pTrack->getPlayed());
    } else if (fieldIndex(LIBRARYTABLE_TIMESPLAYED) == column) {
        return QVariant(pTrack->getTimesPlayed());
    } else if (fieldIndex(LIBRARYTABLE_RATING) == column) {
        return pTrack->getRating();
    } else if (fieldIndex(LIBRARYTABLE_KEY) == column) {
        return pTrack->getKey();
    }
    return QVariant();
}

void BaseSqlTableModel::setTrackValueForColumn(TrackPointer pTrack, int column, QVariant value) {
    // TODO(XXX) Qt properties could really help here.
    if (fieldIndex(LIBRARYTABLE_ARTIST) == column) {
        pTrack->setArtist(value.toString());
    } else if (fieldIndex(LIBRARYTABLE_TITLE) == column) {
        pTrack->setTitle(value.toString());
    } else if (fieldIndex(LIBRARYTABLE_ALBUM) == column) {
        pTrack->setAlbum(value.toString());
    } else if (fieldIndex(LIBRARYTABLE_YEAR) == column) {
        pTrack->setYear(value.toString());
    } else if (fieldIndex(LIBRARYTABLE_GENRE) == column) {
        pTrack->setGenre(value.toString());
    } else if (fieldIndex(LIBRARYTABLE_FILETYPE) == column) {
        pTrack->setType(value.toString());
    } else if (fieldIndex(LIBRARYTABLE_TRACKNUMBER) == column) {
        pTrack->setTrackNumber(value.toString());
    } else if (fieldIndex(LIBRARYTABLE_LOCATION) == column) {
        pTrack->setLocation(value.toString());
    } else if (fieldIndex(LIBRARYTABLE_COMMENT) == column) {
        pTrack->setComment(value.toString());
    } else if (fieldIndex(LIBRARYTABLE_DURATION) == column) {
        pTrack->setDuration(value.toInt());
    } else if (fieldIndex(LIBRARYTABLE_BITRATE) == column) {
        pTrack->setBitrate(value.toInt());
    } else if (fieldIndex(LIBRARYTABLE_BPM) == column) {
        //QVariant::toFloat needs >= QT 4.6.x
        pTrack->setBpm((float) value.toDouble());
    } else if (fieldIndex(LIBRARYTABLE_PLAYED) == column) {
        pTrack->setPlayed(value.toBool());
    } else if (fieldIndex(LIBRARYTABLE_TIMESPLAYED) == column) {
        pTrack->setTimesPlayed(value.toInt());
    } else if (fieldIndex(LIBRARYTABLE_RATING) == column) {
        StarRating starRating = qVariantValue<StarRating>(value);
        pTrack->setRating(starRating.starCount());
    } else if (fieldIndex(LIBRARYTABLE_KEY) == column) {
        pTrack->setKey(value.toString());
    }
}

QVariant BaseSqlTableModel::getBaseValue(const QModelIndex& index, int role) const {
    if (role != Qt::DisplayRole &&
        role != Qt::ToolTipRole &&
        role != Qt::EditRole) {
        return QVariant();
    }

    int row = index.row();
    int column = index.column();

    if (row < 0 || row >= m_rowToTrackId.size()) {
        return QVariant();
    }

    int trackId = m_rowToTrackId[row];

    return getTrackValueForColumn(trackId, column);
}

QString BaseSqlTableModel::filterClause() const {
    QStringList queryFragments;

    if (!m_currentSearchFilter.isNull() && m_currentSearchFilter != "") {
        queryFragments << QString("(%1)").arg(m_currentSearchFilter);
    }

    if (!m_currentSearch.isNull() && m_currentSearch != "") {
        QStringList tokens = m_currentSearch.split(" ");
        QSqlField search("search", QVariant::String);

        QStringList tokenFragments;
        foreach (QString token, tokens) {
            search.setValue("%" + token + "%");
            QString escapedToken = database().driver()->formatValue(search);

            QStringList columnFragments;
            foreach (QString column, m_searchColumns) {
                columnFragments << QString("%1 LIKE %2").arg(column).arg(escapedToken);
            }
            tokenFragments << QString("(%1)").arg(columnFragments.join(" OR "));
        }
        queryFragments << QString("(%1)").arg(tokenFragments.join(" AND "));
    }

    if (queryFragments.size() > 0)
        return "WHERE " + queryFragments.join(" AND ");
    return "";
}

QString BaseSqlTableModel::orderByClause() const {
    // This is all stolen from QSqlTableModel::orderByClause(), just rigged to
    // sort case-insensitively.

    // TODO(rryan) I couldn't get QSqlRecord to work without exec'ing this damn
    // query. Need to find out how to make it work without exec()'ing and remove
    // this.
    QSqlQuery query(m_database);
    QString queryString = QString("SELECT %1 FROM %2 LIMIT 1").arg(m_columnNamesJoined).arg(m_tableName);
    query.prepare(queryString);
    query.exec();

    QString s;
    QSqlField f = query.record().field(m_iSortColumn);
    if (!f.isValid()) {
        if (sDebug) {
            qDebug() << "field not valid";
        }
        return QString();
    }

    QString table = m_tableName;
    QString field = m_database.driver()->escapeIdentifier(f.name(), QSqlDriver::FieldName);

    //QString field = m_columnNames[m_iSortColumn];

    s.append(QLatin1String("ORDER BY "));
    QString sort_field = QString("%1.%2").arg(table).arg(field);

    // If the field is a string, sort using its lowercase form so sort is
    // case-insensitive.
    QVariant::Type type = f.type();

    // TODO(XXX) Instead of special-casing tracknumber here, we should ask the
    // child class to format the expression for sorting.
    if (sort_field.contains("tracknumber")) {
        sort_field = QString("cast(%1 as integer)").arg(sort_field);
    } else if (type == QVariant::String) {
        sort_field = QString("lower(%1)").arg(sort_field);
    }
    s.append(sort_field);

    s += m_eSortOrder == Qt::AscendingOrder ? QLatin1String(" ASC") : QLatin1String(" DESC");
    return s;
}
