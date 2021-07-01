//*@@@+++@@@@******************************************************************
//
// Microsoft Windows Media Foundation
// Copyright (C) Microsoft Corporation. All rights reserved.
//
//*@@@---@@@@******************************************************************

#pragma once

#include <windows.h>

template <class T>
class CTEntryArray
{
public:
    typedef CTEntryArray<T> _EntryArray;

    CTEntryArray();
    ~CTEntryArray();

    // Attributes
    DWORD GetSize() const;
    HRESULT SetSize( DWORD nNewSize, DWORD nGrowBy = 0xffffffff );
    HRESULT Copy( const T* pSrc, DWORD nSrc );
    HRESULT Copy( const CTEntryArray &src );
    HRESULT Set( const T* pSrc, DWORD nSrc )
    {
        return Copy( pSrc, nSrc );
    }
    HRESULT Set( const CTEntryArray &src )
    {
        return Copy( src );
    }

    // Clean up
    void FreeExtra();
    void RemoveAll();

    // Accessing elements
    BOOL GetAt( DWORD nIndex, T * pElement ) const;
    BOOL GetAt( DWORD nIndex, T ** ppElement );

    // Potentially growing the array
    HRESULT SetAtGrow( DWORD nIndex, const T & NewElement, DWORD dwGrowBy = 0 );
    HRESULT Add( const T & NewElement, DWORD * pdwIndex = nullptr, DWORD dwGrowBy = 0 );

    // Operations that move elements around
    HRESULT InsertAt( DWORD nIndex, const T & NewElement );
    HRESULT RemoveAt( DWORD nIndex, DWORD nCount = 1 );

    T & operator[]( DWORD nIndex );
    const T & operator[]( DWORD nIndex ) const;
    T* P()
    {
        return m_pData;
    }
    const T* P() const
    {
        return m_pData;
    }

private:
    BOOL SetAt( DWORD nIndex, const T & NewElement );

    const _EntryArray & operator = ( const _EntryArray & ) = delete;
    CTEntryArray( const _EntryArray& ) = delete;

protected:
    T* m_pData;   // the actual array of data
    DWORD m_nSize;     // # of elements (upperBound - 1)
    DWORD m_nMaxSize;  // max allocated
    DWORD m_nGrowBy;   // grow amount
};

typedef CTEntryArray<BYTE> CTByteArray;

template <class T>
DWORD CTEntryArray<T>::GetSize() const
{
    return( m_nSize );
}

template <class T>
void CTEntryArray<T>::RemoveAll()
{
    (void)SetSize( 0 );
}

template <class T>
HRESULT CTEntryArray<T>::Copy( const T* pSrc, DWORD nSrc )
{
    HRESULT hr = S_OK;

    hr = SetSize( nSrc );

    if( SUCCEEDED( hr ) && ( 0 != nSrc ) )
    {
        memcpy( m_pData, pSrc, nSrc * sizeof( T ) );
    }

    return hr;
}

template <class T>
HRESULT CTEntryArray<T>::Copy( const CTEntryArray &src )
{
    return Copy( src.P(), src.GetSize() );
}

template <class T>
BOOL CTEntryArray<T>::GetAt( DWORD nIndex, T * pElement ) const
{
    if( nIndex >= m_nSize )
    {
        return( FALSE );
    }

    *pElement = m_pData[ nIndex ];

    return( TRUE );
}

template <class T>
BOOL CTEntryArray<T>::GetAt( DWORD nIndex, T ** ppElement )
{
    if( nIndex >= m_nSize )
    {
        return( FALSE );
    }

    *ppElement = &m_pData[ nIndex ];

    return( TRUE );
}

template <class T>
BOOL CTEntryArray<T>::SetAt( DWORD nIndex, const T & NewElement )
{
    if( nIndex >= m_nSize )
    {
        return( FALSE );
    }

    m_pData[ nIndex ] = NewElement;

    return( TRUE );
}

template <class T>
HRESULT CTEntryArray<T>::Add( const T & NewElement, DWORD *pdwIndex, DWORD dwGrowBy )
{
    if( pdwIndex != nullptr )
    {
        *pdwIndex = m_nSize;
    }

    return SetAtGrow( m_nSize, NewElement, dwGrowBy );
}

template <class T>
T & CTEntryArray<T>::operator[]( DWORD nIndex )
{
    return m_pData[ nIndex ];
}

template <class T>
const T & CTEntryArray<T>::operator[]( DWORD nIndex ) const
{
    return m_pData[ nIndex ];
}

template <class T>
CTEntryArray<T>::CTEntryArray()
{
    m_pData = nullptr;
    m_nSize = m_nMaxSize = m_nGrowBy = 0;
}

template <class T>
CTEntryArray<T>::~CTEntryArray()
{
    delete[] m_pData;
}

template <class T>
HRESULT CTEntryArray<T>::SetSize( DWORD nNewSize, DWORD nGrowBy )
{
    HRESULT hr = S_OK;
    if( nGrowBy != 0xffffffff )
    {
        m_nGrowBy = nGrowBy;  // set new size
    }

    if( nNewSize == 0 )
    {
        //
        //  Just set the size to 0
        //  No need to deallocate immediate in case the class gets reused.
        //
        m_nSize = 0;
    }
    else if( nullptr == m_pData )
    {
        // create one with exact size
        m_pData = new T[ nNewSize ];

        if( nullptr == m_pData )
        {
            hr = E_OUTOFMEMORY;
        }
        else
        {
            ZeroMemory( m_pData, nNewSize * sizeof( T ) );  // zero fill
            m_nSize = m_nMaxSize = nNewSize;
        }
    }
    else if( nNewSize <= m_nMaxSize )
    {
        // it fits
        if( nNewSize > m_nSize )
        {
            // initialize the new elements
            ZeroMemory( &m_pData[ m_nSize ], ( nNewSize - m_nSize ) * sizeof( T ) );
        }

        m_nSize = nNewSize;
    }
    else
    {
        // otherwise, grow array
        nGrowBy = m_nGrowBy;

        if( 0 == nGrowBy )
        {
            // heuristically determine growth when nGrowBy == 0
            //  (this avoids heap fragmentation in many situations)
            nGrowBy = min( 1024, max( 4, m_nSize / 8 ) );
        }

        DWORD nNewMax;
        if( nNewSize < m_nMaxSize + nGrowBy )
        {
            nNewMax = (DWORD)( m_nMaxSize + nGrowBy );  // granularity
        }
        else
        {
            nNewMax = nNewSize;  // no slush
        }

        T * pNewData = new T[ nNewMax ];

        if( nullptr == pNewData )
        {
            hr = E_OUTOFMEMORY;
        }
        else
        {
            // copy new data from old
            memcpy( pNewData, m_pData, m_nSize * sizeof( T ) );

            // construct remaining elements
            ZeroMemory( &pNewData[ m_nSize ], ( nNewSize - m_nSize ) * sizeof( T ) );

            // get rid of old stuff (note: destructors WILL be called)
            delete[] m_pData;
            m_pData = pNewData;
            m_nSize = nNewSize;
            m_nMaxSize = nNewMax;
        }
    }

    return hr;
}

template <class T>
void CTEntryArray<T>::FreeExtra()
{
    if( m_nSize != m_nMaxSize )
    {
        // shrink to desired size
        void **pNewData = nullptr;
        if( m_nSize != 0 )
        {
            pNewData = ( void** ) new BYTE[ m_nSize * sizeof( void* ) ];
            // copy new data from old
            memcpy( pNewData, m_pData, m_nSize * sizeof( void* ) );
        }

        // get rid of old stuff (note: no destructors called)
        delete[] m_pData;
        m_pData    = pNewData;
        m_nMaxSize = m_nSize;
    }
}

template <class T>
HRESULT CTEntryArray<T>::SetAtGrow( DWORD nIndex, const T & NewElement, DWORD dwGrowBy )
{
    HRESULT hr = S_OK;

    if( nIndex >= m_nSize )
    {
        hr = SetSize( nIndex + 1, dwGrowBy );
    }

    if( SUCCEEDED( hr ) )
    {
        m_pData[ nIndex ] = NewElement;
    }

    return hr;
}

template <class T>
HRESULT CTEntryArray<T>::InsertAt( DWORD nIndex, const T & NewElement )
{
    HRESULT hr = S_OK;
    if( nIndex >= m_nSize )
    {
        // adding after the end of the array
        hr = SetSize( nIndex + 1 );  // grow so nIndex is valid
    }
    else
    {
        // inserting in the middle of the array
        int nOldSize = m_nSize;

        hr = SetSize( m_nSize + 1 );  // grow it to new size

        if( SUCCEEDED( hr ) )
        {
            // shift old data up to fill gap
            MoveMemory( &m_pData[ nIndex + 1 ], &m_pData[ nIndex ], ( nOldSize - nIndex ) * sizeof( T ) );
        }
    }

    if( SUCCEEDED( hr ) )
    {
        // insert new value in the gap
        m_pData[ nIndex ] = NewElement;
    }

    return hr;
}

template <class T>
HRESULT CTEntryArray<T>::RemoveAt( DWORD nIndex, DWORD nCount )
{
    HRESULT hr = S_OK;

    if( nCount > m_nSize || nIndex > m_nSize - nCount )
    {
        hr = E_UNEXPECTED;
    }
    else
    {
        // just remove a range
        DWORD nMoveCount = m_nSize - ( nIndex + nCount );

        if( nMoveCount > 0 )
        {
            MoveMemory( &m_pData[ nIndex ], &m_pData[ nIndex + nCount ], nMoveCount * sizeof( T ) );
        }

        m_nSize -= nCount;
    }

    return hr;
}

template <class T>
class CTUnkArray
{
public:
    typedef CTUnkArray<T> _UnkArray;

    CTUnkArray() : m_Array()
    {
    }

    ~CTUnkArray()
    {
        RemoveAll();
    }

    // Attributes
    DWORD GetSize() const
    {
        return m_Array.GetSize();
    }

    HRESULT SetSize( DWORD nNewSize, DWORD nGrowBy = 0xffffffff )
    {
        return m_Array.SetSize( nNewSize, nGrowBy );
    }

    HRESULT Copy( const CTUnkArray<T> &src )
    {
        HRESULT hr = S_OK;
        RemoveAll();
        for( DWORD n = 0; SUCCEEDED( hr ) && n < src.GetSize(); n++ )
        {
            hr = Add( src.GetAt( n ) );
        }
        return hr;
    }

    // Clean up
    void RemoveAll()
    {
        DWORD n;
        T * p = nullptr;
        for( n = 0; n < m_Array.GetSize(); n++ )
        {
            m_Array.GetAt( n, &p );
            if( p != nullptr )
            {
                p->Release();
            }
        }
        m_Array.RemoveAll();
    }

    // Accessing elements
    T * GetAt( DWORD nIndex ) const
    {
        T * p;
        if( !m_Array.GetAt( nIndex, &p ) )
        {
            return nullptr;
        }

        return p;
    }

    HRESULT SetAtGrow( DWORD nIndex, T * NewElement )
    {
        HRESULT hr = S_OK;

        T * OldElement = GetAt( nIndex );

        hr = m_Array.SetAtGrow( nIndex, NewElement );

        if( SUCCEEDED( hr ) )
        {
            if( NewElement )
            {
                NewElement->AddRef();
            }

            if( OldElement )
            {
                OldElement->Release();
            }
        }

        return hr;
    }

    HRESULT Add( T * NewElement )
    {
        HRESULT hr = m_Array.Add( NewElement );
        if( SUCCEEDED( hr ) && NewElement )
        {
            NewElement->AddRef();
        }

        return hr;
    }

    HRESULT Add( _UnkArray & From )
    {
        HRESULT hr = S_OK;

        DWORD n;
        for( n=0; SUCCEEDED( hr ) && n < From.GetSize(); n++ )
        {
            hr = Add( From.GetAt( n ) );
        }

        return hr;
    }

    // Operations that move elements around
    HRESULT InsertAt( DWORD nIndex, T * NewElement )
    {
        HRESULT hr = m_Array.InsertAt( nIndex, NewElement );
        if( SUCCEEDED( hr ) && NewElement )
        {
            NewElement->AddRef();
        }

        return hr;
    }

    HRESULT RemoveAt( DWORD nIndex )
    {
        HRESULT hr = S_OK;
        T * p;
        if( m_Array.GetSize() <= nIndex
         || !m_Array.GetAt( nIndex, &p ) )
        {
            hr = E_UNEXPECTED;
        }
        else
        {
            hr = m_Array.RemoveAt( nIndex, 1 );
            if( SUCCEEDED( hr ) )
            {
                p->Release();
            }
        }
        return hr;
    }

    T*& operator[]( DWORD nIndex )
    {
        return m_Array[ nIndex ];
    }
    T** P()
    {
        return m_Array.P();
    }

protected:
    BOOL SetAt( DWORD nIndex, T * NewElement )
    {
        T * OldElement = GetAt( nIndex );

        BOOL fRet = m_Array.SetAt( nIndex, NewElement );

        if( fRet )
        {
            if( NewElement )
            {
                NewElement->AddRef();
            }

            if( OldElement )
            {
                OldElement->Release();
            }
        }

        return( fRet );
    }

    typedef CTEntryArray<T*> _Array;

    _Array m_Array;
};

class CTDWORDArray : public CTEntryArray<DWORD> {};

template <class NODEBLOCK, class NODE, long _FixedSize>
class CTNodePool
{
public:
    CTNodePool()
    {
        m_pFree = nullptr;
        m_pBlocks = nullptr;
        m_cEntryAllocation = 0;
    }

    void Init( DWORD cEntryAllocation )
    {
        if( m_pBlocks != nullptr )
        {
            return;
        }

        m_cEntryAllocation = cEntryAllocation;
        //
        // add nodes from fixed block to free stack
        //
        m_pBlocks = (NODEBLOCK*)_internal.m_FixedBlock;
        m_pBlocks->pNext = nullptr;

        for( DWORD dw = 0; dw < _FixedSize; dw++ )
        {
            //
            // use placement new to construct NODE objects in preallocated memory
            //
            NODE *pNode = new( &m_pBlocks->Node[ dw ] ) NODE;
            ReleaseNode( pNode );
        }
    }

    void Cleanup()
    {
        HANDLE hHeap = ::GetProcessHeap();

        //
        // Note that NODE::~NODE is called directly because we need to destruct the NODE objects for cleanup
        // to happen but can't call delete because we used placement new for construction
        //

        for( ; m_pBlocks; )
        {
            NODEBLOCK * p = m_pBlocks->pNext;

            if( (BYTE*)m_pBlocks != _internal.m_FixedBlock )
            {
                for( DWORD dw=0; dw < m_cEntryAllocation; dw++ )
                {
                    m_pBlocks->Node[ dw ].~NODE();
                }
                ::HeapFree( hHeap, 0, m_pBlocks );
            }
            else
            {
                for( DWORD dw=0; dw < _FixedSize; dw++ )
                {
                    m_pBlocks->Node[ dw ].~NODE();
                }
            }
            m_pBlocks = p;
        }
    }

    void ReleaseNode( NODE * pNode )
    {
        pNode->pNext = m_pFree;
        m_pFree = pNode;
    }

    HRESULT AcquireNode( NODE ** ppNode )
    {
        if( nullptr == m_pFree )
        {
            NODEBLOCK * pBlock;

            pBlock = ( NODEBLOCK* )::HeapAlloc( ::GetProcessHeap(),
                                                   0,
                                                   sizeof( NODEBLOCK ) + ( m_cEntryAllocation - 1 ) * sizeof( NODE ) );
            if( nullptr == pBlock )
            {
                return E_OUTOFMEMORY;
            }

            for( DWORD dw=0; dw < m_cEntryAllocation; dw++ )
            {
                //
                // use placement new to construct NODE objects in preallocated memory
                //
                NODE *pNode = new( &pBlock->Node[ dw ] ) NODE;
                ReleaseNode( pNode );
            }

            pBlock->pNext = m_pBlocks;
            m_pBlocks = pBlock;
        }

        ( *ppNode ) = m_pFree;
        m_pFree = m_pFree->pNext;

        return NOERROR;
    }

protected:
    NODE *m_pFree;
    NODEBLOCK *m_pBlocks; // list of blocks
    DWORD m_cEntryAllocation;

    //
    // fixed block of nodes for small lists to prevent new/delete
    //
    union
    {
        // tell compiler to align m_FixedBlock
        struct
        {
            void * m_pvAligment;
        } _internal_struct;
        BYTE  m_FixedBlock[ sizeof( NODEBLOCK ) + sizeof( NODE )*( _FixedSize - 1 ) ];
    } _internal;
};

typedef void * LISTPOS;
class CVPtrList
{
public:
    CVPtrList()
    {
        m_cEntries = 0;
        m_pHead = m_pTail = nullptr;
    }

    ~CVPtrList()
    {
        RemoveAll();
        m_NodePool.Cleanup();
    }

    HRESULT Initialize( DWORD cInitialEntryAllocation );

    void RemoveAll();

    // count of elements
    DWORD GetCount() const;
    BOOL IsEmpty() const;

    // peek at head or tail
    BOOL GetHead( _Out_ void **ppHead ) const;
    BOOL GetTail( _Out_ void **ppTail ) const;

    // get head or tail (and remove it)
    BOOL RemoveHead( _Out_ void **ppOldHead );
    BOOL RemoveTail( _Out_ void **ppOldTail );

    // add before head or after tail
    LISTPOS AddHead( void *pNewHead );
    LISTPOS AddTail( void *pNewTail );

    // add another list of elements before head or after tail
    void AddHead( CVPtrList *pNewList );
    void AddTail( CVPtrList *pNewList );

    // iteration
    LISTPOS GetHeadPosition() const;
    LISTPOS GetTailPosition() const;
    BOOL GetNext( LISTPOS& rPos, _Out_ void **ppNext ) const;
    BOOL GetPrev( LISTPOS& rPos, _Out_ void **ppPrev ) const;

    // getting/modifying an element at a given position
    BOOL GetAt( LISTPOS pos, _Out_ void **ppElement ) const;
    void SetAt( LISTPOS pos, void *pNewElement );
    void RemoveAt( LISTPOS pos );
    void MoveToHead( LISTPOS pos );
    void MoveToTail( LISTPOS pos );

    // inserting before or after a given position
    LISTPOS InsertBefore( LISTPOS pos, void *pNewElement );
    LISTPOS InsertAfter( LISTPOS pos, void *pNewElement );

    // helper functions (note: O(n) speed)
    LISTPOS Find( void *pSearchValue, LISTPOS startAfter = nullptr ) const;
    LISTPOS FindIndex( DWORD nIndex ) const;

    class CComparator
    {
    public:
        virtual BOOL IsLessThan( void* pData1, void* pData2 ) = 0;
    private:
        virtual ~CComparator() = 0;
    };

    BOOL Sort( CComparator* pComparator );

private:

    struct NODE
    {
        void *pValue;
        NODE *pNext;
        NODE *pPrev;
    };

    struct NODEBLOCK
    {
        NODEBLOCK *pNext;
        NODE Node[ 1 ];
    };

    CTNodePool<NODEBLOCK, NODE, 16> m_NodePool;

    int Compare( const void *pValue1, const void *pValue2 ) const;

    NODE *m_pHead;
    NODE *m_pTail;
    DWORD m_cEntries;
};

template<class Type>
class CTPtrList
{
public:
    // Type casting wrapper class for CVPtrList

    HRESULT Initialize( DWORD cInitialEntryAllocation );

    void RemoveAll();

    // count of elements
    DWORD GetCount() const;
    BOOL IsEmpty() const;

    // peek at head or tail
    BOOL GetHead( _Out_ Type **ppHead ) const;
    BOOL GetTail( _Out_ Type **ppTail ) const;

    // get head or tail (and remove it)
    BOOL RemoveHead( _Out_ Type **ppOldHead );
    BOOL RemoveTail( _Out_ Type **ppOldTail );

    // add before head or after tail
    LISTPOS AddHead( Type *pNewHead );
    LISTPOS AddTail( Type *pNewTail );

    // add another list of elements before head or after tail
    void AddHead( CTPtrList<Type> *pNewList );
    void AddTail( CTPtrList<Type> *pNewList );

    // iteration
    LISTPOS GetHeadPosition() const;
    LISTPOS GetTailPosition() const;
    BOOL GetNext( LISTPOS& rLISTPOS, _Out_ Type **ppNext ) const;
    BOOL GetPrev( LISTPOS& rLISTPOS, _Out_ Type **ppPrev ) const;

    // getting/modifying an element at a given LISTPOS
    BOOL GetAt( LISTPOS pos, _Out_ Type **ppElement ) const;
    void SetAt( LISTPOS pos, Type *pNewElement );
    void RemoveAt( LISTPOS pos );
    void MoveToHead( LISTPOS pos );
    void MoveToTail( LISTPOS pos );

    // inserting before or after a given LISTPOS
    LISTPOS InsertBefore( LISTPOS pos, Type *pNewElement );
    LISTPOS InsertAfter( LISTPOS pos, Type *pNewElement );

    // helper functions (note: O(n) speed)
    LISTPOS Find( Type *pSearchValue, LISTPOS startAfter = nullptr ) const;
    LISTPOS FindIndex( DWORD nIndex ) const;

    class CTComparator
    {
    public:
        virtual BOOL IsLessThan( Type* p1, Type* p2 ) = 0;
    };

    BOOL Sort( CTComparator* pComparator );
private:
    CVPtrList m_List;
};

template<class Type>
HRESULT CTPtrList<Type>::Initialize( DWORD cInitialEntryAllocation )
{
    return m_List.Initialize( cInitialEntryAllocation );
}

template<class Type>
void CTPtrList<Type>::RemoveAll()
{
    m_List.RemoveAll();
}

template<class Type>
DWORD CTPtrList<Type>::GetCount() const
{
    return m_List.GetCount();
}

template<class Type>
BOOL CTPtrList<Type>::IsEmpty() const
{
    return m_List.IsEmpty();
}

template<class Type>
LISTPOS CTPtrList<Type>::GetHeadPosition() const
{
    return m_List.GetHeadPosition();
}

template<class Type>
LISTPOS CTPtrList<Type>::GetTailPosition() const
{
    return m_List.GetTailPosition();
}

template<class Type>
void CTPtrList<Type>::RemoveAt( LISTPOS pos )
{
    m_List.RemoveAt( pos );
}

template<class Type>
void CTPtrList<Type>::MoveToHead( LISTPOS pos )
{
    m_List.MoveToHead( pos );
}

template<class Type>
void CTPtrList<Type>::MoveToTail( LISTPOS pos )
{
    m_List.MoveToTail( pos );
}

template<class Type>
BOOL CTPtrList<Type>::GetHead( _Out_ Type **ppHead ) const
{
    return( m_List.GetHead( (void **)ppHead ) );
}

template<class Type>
BOOL CTPtrList<Type>::GetTail( _Out_ Type **ppTail ) const
{
    return( m_List.GetTail( (void **)ppTail ) );
}

template<class Type>
BOOL CTPtrList<Type>::RemoveHead( _Out_ Type **ppOldHead )
{
    return( m_List.RemoveHead( (void **)ppOldHead ) );
}

template<class Type>
BOOL CTPtrList<Type>::RemoveTail( _Out_ Type **ppOldTail )
{
    return( m_List.RemoveTail( (void **)ppOldTail ) );
}

template<class Type>
LISTPOS CTPtrList<Type>::AddHead( Type *pNewHead )
{
    return( m_List.AddHead( (void *)pNewHead ) );
}

template<class Type>
LISTPOS CTPtrList<Type>::AddTail( Type *pNewTail )
{
    return( m_List.AddTail( (void *)pNewTail ) );
}

template<class Type>
void CTPtrList<Type>::AddHead( CTPtrList<Type> *pNewList )
{
    m_List.AddHead( (CVPtrList *)pNewList );
}

template<class Type>
void CTPtrList<Type>::AddTail( CTPtrList<Type> *pNewList )
{
    m_List.AddTail( (CVPtrList *)pNewList );
}

template<class Type>
BOOL CTPtrList<Type>::GetNext( LISTPOS& rLISTPOS, _Out_ Type **ppNext ) const
{
    return( m_List.GetNext( rLISTPOS, (void **)ppNext ) );
}

template<class Type>
BOOL CTPtrList<Type>::GetPrev( LISTPOS& rLISTPOS, _Out_ Type **ppPrev ) const
{
    return( m_List.GetPrev( rLISTPOS, (void **)ppPrev ) );
}

template<class Type>
BOOL CTPtrList<Type>::GetAt( LISTPOS pos, _Out_ Type **ppElement ) const
{
    return( m_List.GetAt( pos, (void **)ppElement ) );
}

template<class Type>
void CTPtrList<Type>::SetAt( LISTPOS pos, Type *pNewElement )
{
    m_List.SetAt( pos, (void *)pNewElement );
}

template<class Type>
LISTPOS CTPtrList<Type>::InsertBefore( LISTPOS pos, Type *pNewElement )
{
    return( m_List.InsertBefore( pos, (void *)pNewElement ) );
}

template<class Type>
LISTPOS CTPtrList<Type>::InsertAfter( LISTPOS pos, Type *pNewElement )
{
    return( m_List.InsertAfter( pos, (void *)pNewElement ) );
}

template<class Type>
LISTPOS CTPtrList<Type>::Find( Type *pSearchValue, LISTPOS startAfter ) const
{
    return( m_List.Find( (void *)pSearchValue, startAfter ) );
}

template<class Type>
BOOL CTPtrList<Type>::Sort( CTComparator* pComparator )
{
    return( m_List.Sort( pComparator ) );
}



