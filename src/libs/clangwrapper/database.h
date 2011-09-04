#ifndef MULTIINDEXCONTAINER_H
#define MULTIINDEXCONTAINER_H

#include <QtCore/QList>
#include <QtCore/QLinkedList>
#include <QtCore/QHash>
#include <QtCore/QDebug>

namespace Clang {

/*
 * NOTE: This is a work in progress...
 *
 * This data-structure is container which can be indexed by multiple keys. It's used by the
 * indexer to keep track of the symbols in the project, which can be accessed either through
 * the file to which they belong or through the type of the symbol (a class, enum, etc...),
 * or a combination of them.
 *
 * The implementation is not complete yet... and maybe a bit rudimentary. The current design
 * has some limitations. For instance, it remains to provide a way for instantiating keys with
 * the same type (probably associating integers ids to them) and actually re-thinking the way
 * to scale a few operations for many keys.
 *
 * The base template is currently not implemented. Only the specialization for 2 keys is
 * implemented and being used by the indexer. But ideally this should be just a matter of
 * writing analogous code for the other specializations. In fact we could use variadic
 * macros to auxiliate that (although I think variadic macros are part of C99 but not of
 * current C++ so one probably needs to manually handle the platforms).
 */


// Empty Key to be used as default template argument.
struct EmptyKey {};

// Function template that needs to be specialized by key types.
template <class Key_T, class Return_T, class Data_T>
Return_T getKey(const Data_T &);

// Utility...
template <class Data_T,
          class DataIterator_T,
          class Hash_T>
QList<Data_T> valuesGeneric(const Hash_T &h)
{
    QList<Data_T> all;
    typename Hash_T::const_iterator it = h.begin();
    typename Hash_T::const_iterator eit = h.end();
    for (; it != eit; ++it) {
        QList<DataIterator_T> dataList = *it;
        foreach (DataIterator_T dataIt, dataList)
            all.append(*dataIt);
    }
    return all;
}

template <class Data_T,
          class DataIterator_T,
          class KeyX_Value_T,
          class Hash_T>
QList<Data_T> valuesGeneric(const KeyX_Value_T &x,
                            const Hash_T &h)
{
    QList<Data_T> all;
    QList<DataIterator_T> dataList = h.value(x);
    foreach (DataIterator_T dataIt, dataList)
        all.append(*dataIt);
    return all;
}

// @TODO: More generic functions for other operations...


// Base template for 3 keys
template <class Data_T,
          class Key0_T,
          class Key1_T,
          class Key2_T>
class DatabaseBase
{
    typedef Data_T DataType;
    typedef typename QLinkedList<Data_T>::iterator DataIteratorType;
    typedef Key0_T Key0Type;
    typedef Key1_T Key1Type;
    typedef Key2_T Key2Type;
    typedef typename Key0_T::ValueType Key0ValueType;
    typedef typename Key1_T::ValueType Key1ValueType;
    typedef typename Key2_T::ValueType Key2ValueType;

protected:
    // @TODO: See note in the beginning of the file.
    void createIndex(DataIteratorType it) { Q_UNUSED(it) }
    QList<DataIteratorType> removeIndex(Key0Type, const Key0ValueType &v) { Q_UNUSED(v); }
    QList<DataIteratorType> removeIndex(Key1Type, const Key1ValueType &v) { Q_UNUSED(v); }
    QList<DataIteratorType> removeIndex(Key2Type, const Key2ValueType &v) { Q_UNUSED(v); }
    void clearAllIndexes() {}
};


// Specialization for 2 keys
template <class Data_T,
          class Key0_T,
          class Key1_T>
class DatabaseBase <
        Data_T,
        Key0_T,
        Key1_T,
        EmptyKey>
{
    typedef Data_T DataType;
    typedef typename QLinkedList<Data_T>::iterator DataIteratorType;
    typedef Key0_T Key0Type;
    typedef Key1_T Key1Type;
    typedef typename Key0_T::ValueType Key0ValueType;
    typedef typename Key1_T::ValueType Key1ValueType;
    typedef QHash<Key0ValueType, QList<DataIteratorType> > Hash0Type;
    typedef QHash<Key1ValueType, QList<DataIteratorType> > Hash1Type;
    typedef QHash<Key0ValueType, Hash1Type> Hash01Type;
    typedef QHash<Key1ValueType, Hash0Type> Hash10Type; // @TODO: Change...

public:
    QList<Data_T> values(Key0Type, const Key0ValueType &v) const
    {  return valuesGeneric<DataType, DataIteratorType>(m_index0.value(v)); }

    QList<Data_T> values(Key0Type, const Key0ValueType &v,
                         Key1Type, const Key1ValueType &vv) const
    { return valuesGeneric<DataType, DataIteratorType>(vv, m_index0.value(v)); }

    QList<Data_T> values(Key1Type, const Key1ValueType &v) const
    { return valuesGeneric<DataType, DataIteratorType>(m_index1.value(v)); }

    QList<Data_T> values(Key1Type, const Key1ValueType &v,
                         Key0Type, const Key0ValueType &vv) const
    { return valuesGeneric<DataType, DataIteratorType>(vv, m_index1.value(v)); }

protected:
    void createIndex(DataIteratorType it)
    {
        m_index0[getKey<Key0Type, Key0ValueType>(*it)]
                [getKey<Key1Type, Key1ValueType>(*it)].append(it);
        m_index1[getKey<Key1Type, Key1ValueType>(*it)]
                [getKey<Key0Type, Key0ValueType>(*it)].append(it);
    }

    QList<DataIteratorType> removeIndex(Key0Type, const Key0ValueType &v)
    {
        QList<DataIteratorType> result;
        Hash1Type h = m_index0.take(v);
        typename Hash1Type::iterator it = h.begin();
        typename Hash1Type::iterator eit = h.end();
        for (; it != eit; ++it) {
            QList<DataIteratorType> dataList = *it;
            foreach (DataIteratorType dataIt, dataList) {
                result.append(dataIt);
                m_index1[getKey<Key1Type, Key1ValueType>(*dataIt)]
                        .remove(getKey<Key0Type, Key0ValueType>(*dataIt));
            }
        }
        return result;
    }

    QList<DataIteratorType> removeIndex(Key1Type, const Key1ValueType &v)
    {
        QList<DataIteratorType> result;
        Hash0Type h = m_index1.take(v);
        typename Hash0Type::iterator it = h.begin();
        typename Hash0Type::iterator eit = h.end();
        for (; it != eit; ++it) {
            QList<DataIteratorType> dataList = *it;
            foreach (DataIteratorType dataIt, dataList) {
                result.append(dataIt);
                m_index0[getKey<Key0Type, Key0ValueType>(*dataIt)]
                        .remove(getKey<Key1Type, Key1ValueType>(*dataIt));
            }
        }
        return result;
    }

    void clearAllIndexes()
    {
        m_index0.clear();
        m_index1.clear();
    }

private:
    Hash01Type m_index0;
    Hash10Type m_index1;
};

//-----------------------------------------------------------------------------
// The database...
//-----------------------------------------------------------------------------
template <class Data_T,
          class Key0_T,
          class Key1_T,
          class Key2_T = EmptyKey>
class Database
        : public DatabaseBase<Data_T,
                              Key0_T,
                              Key1_T,
                              Key2_T>
{
private:
    typedef Data_T DataType;
    typedef typename QLinkedList<Data_T>::iterator DataIteratorType;

public:
    Database() {}

    void insert(const DataType &data)
    {
        DataIteratorType it = m_container.insert(m_container.begin(), data);
        createIndex(it);
    }

    template <class Key_T, class Key_ValueType>
    void remove(const Key_T &KeyT, const Key_ValueType &valueT)
    {
        const QList<DataIteratorType> &iterators = removeIndex(KeyT, valueT);
        foreach (DataIteratorType it, iterators)
            m_container.erase(it);
    }

    void clear()
    {
        m_container.clear();
        this->clearAllIndexes();
    }

private:
    QLinkedList<Data_T> m_container;
};

} // Clang

#endif // MULTIINDEXCONTAINER_H
