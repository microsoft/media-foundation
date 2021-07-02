//*@@@+++@@@@******************************************************************
//
// Microsoft Windows Media Foundation
// Copyright (C) Microsoft Corporation. All rights reserved.
//
//*@@@---@@@@******************************************************************

#include "stdafx.h"

HRESULT CVPtrList::Initialize( DWORD cInitialEntryAllocation )
{
    m_NodePool.Init( cInitialEntryAllocation );
    return NOERROR;
}

void CVPtrList::RemoveAll()
{
    NODE *pNext;

    while( nullptr != m_pHead )
    {
        pNext = m_pHead->pNext;
        m_NodePool.ReleaseNode( m_pHead );
        m_pHead = pNext;
    }

    m_cEntries = 0;
    m_pTail = nullptr;
}

LISTPOS CVPtrList::AddHead( void *pNewHead )
{
    NODE *pNewNode;
    if( FAILED( m_NodePool.AcquireNode( &pNewNode ) ) )
    {
        return( (LISTPOS)nullptr );
    }

    pNewNode->pValue = pNewHead;
    pNewNode->pNext = m_pHead;
    pNewNode->pPrev = nullptr;

    if( nullptr != m_pHead )
    {
        m_pHead->pPrev = pNewNode;
    }
    else
    {
        m_pTail = pNewNode;
    }

    m_pHead = pNewNode;
    m_cEntries++;

    return( (LISTPOS)pNewNode );
}

LISTPOS CVPtrList::AddTail( void *pNewTail )
{
    NODE *pNewNode;
    if( FAILED( m_NodePool.AcquireNode( &pNewNode ) ) )
    {
        return( (LISTPOS)nullptr );
    }

    pNewNode->pValue = pNewTail;
    pNewNode->pNext = nullptr;
    pNewNode->pPrev = m_pTail;

    if( nullptr != m_pTail )
    {
        m_pTail->pNext = pNewNode;
    }
    else
    {
        m_pHead = pNewNode;
    }

    m_pTail = pNewNode;
    m_cEntries++;

    return( (LISTPOS)pNewNode );
}

void CVPtrList::AddHead( CVPtrList *pNewList )
{
    // add a list of same elements to head (maintain order)
    void *pElement;
    LISTPOS pos = pNewList->GetTailPosition();

    while( ( nullptr != pos ) && ( pNewList->GetPrev( pos, &pElement ) ) )
    {
        AddHead( pElement );
    }
}

void CVPtrList::AddTail( CVPtrList *pNewList )
{
    // add a list of same elements
    void *pElement;
    LISTPOS pos = pNewList->GetHeadPosition();

    while( ( nullptr != pos ) && ( pNewList->GetNext( pos, &pElement ) ) )
    {
        AddTail( pElement );
    }
}

BOOL CVPtrList::RemoveHead( _Out_ void **ppOldHead )
{
    if( !ppOldHead || !m_pHead )
    {
        return( FALSE );
    }

    NODE *pOldNode = m_pHead;
    *ppOldHead = pOldNode->pValue;

    m_pHead = pOldNode->pNext;

    if( nullptr != m_pHead )
    {
        m_pHead->pPrev = nullptr;
    }
    else
    {
        m_pTail = nullptr;
    }

    m_NodePool.ReleaseNode( pOldNode );
    m_cEntries--;

    return( TRUE );
}

BOOL CVPtrList::RemoveTail( _Out_ void **ppOldTail )
{
    if( !ppOldTail || !m_pTail )
    {
        return( FALSE );
    }

    NODE *pOldNode = m_pTail;
    *ppOldTail = pOldNode->pValue;

    m_pTail = pOldNode->pPrev;

    if( nullptr != m_pTail )
    {
        m_pTail->pNext = nullptr;
    }
    else
    {
        m_pHead = nullptr;
    }

    m_NodePool.ReleaseNode( pOldNode );
    m_cEntries--;

    return( TRUE );
}

LISTPOS CVPtrList::InsertBefore( LISTPOS pos, void *pNewElement )
{
    if( ( nullptr == pos ) || ( ( (NODE*)pos )->pPrev == nullptr ) )
    {
        return( AddHead( pNewElement ) ); // insert before nothing -> head of the list
    }

    // Insert it before pos
    NODE *pOldNode = (NODE*)pos;

    NODE *pNewNode;
    if( FAILED( m_NodePool.AcquireNode( &pNewNode ) ) )
    {
        return( (LISTPOS)nullptr );
    }

    pNewNode->pValue = pNewElement;
    pNewNode->pPrev = pOldNode->pPrev;
    pNewNode->pNext = pOldNode;

    pOldNode->pPrev->pNext = pNewNode;
    pOldNode->pPrev = pNewNode;

    m_cEntries++;

    return( (LISTPOS)pNewNode );
}

LISTPOS CVPtrList::InsertAfter( LISTPOS pos, void *pNewElement )
{
    if( ( nullptr == pos ) || ( ( (NODE*)pos )->pNext == nullptr ) )
    {
        return( AddTail( pNewElement ) ); // insert after nothing -> tail of the list
    }

    // Insert it after pos
    NODE *pOldNode = (NODE*)pos;

    NODE *pNewNode;
    if( FAILED( m_NodePool.AcquireNode( &pNewNode ) ) )
    {
        return( (LISTPOS)nullptr );
    }

    pNewNode->pValue = pNewElement;
    pNewNode->pPrev = pOldNode;
    pNewNode->pNext = pOldNode->pNext;

    pOldNode->pNext->pPrev = pNewNode;
    pOldNode->pNext = pNewNode;

    m_cEntries++;

    return( (LISTPOS)pNewNode );
}

void CVPtrList::RemoveAt( LISTPOS pos )
{
    NODE *pOldNode = (NODE*)pos;

    // remove pOldNode from list
    if( pOldNode == m_pHead )
    {
        m_pHead = pOldNode->pNext;
    }
    else
    {
        pOldNode->pPrev->pNext = pOldNode->pNext;
    }

    if( pOldNode == m_pTail )
    {
        m_pTail = pOldNode->pPrev;
    }
    else
    {
        pOldNode->pNext->pPrev = pOldNode->pPrev;
    }

    m_NodePool.ReleaseNode( pOldNode );
    m_cEntries--;
}

LISTPOS CVPtrList::FindIndex( DWORD nIndex ) const
{
    if( nIndex >= m_cEntries )
    {
        return( (LISTPOS)nullptr );  // went too far
    }

    NODE *pNode = m_pHead;

    while( nIndex-- )
    {
        pNode = pNode->pNext;
    }

    return( (LISTPOS)pNode );
}

LISTPOS CVPtrList::Find( void *pSearchValue, LISTPOS startAfter ) const
{
    NODE *pNode = (NODE *)startAfter;

    if( nullptr == pNode )
    {
        pNode = m_pHead;  // start at head
    }
    else
    {
        pNode = pNode->pNext;  // start after the one specified
    }

    for( ; nullptr != pNode; pNode = pNode->pNext )
    {
        if( 0 == Compare( pNode->pValue, pSearchValue ) )
        {
            return( (LISTPOS)pNode );
        }
    }

    return( (LISTPOS)nullptr );
}

BOOL CVPtrList::Sort( CVPtrList::CComparator* pComparator )
{
    if( nullptr == pComparator )
    {
        return( FALSE );
    }

    if( m_cEntries >= 2 )
    {
        NODE* pList     = m_pHead->pNext;
        NODE* pStart    = m_pHead;
        NODE* pNewList  = m_pHead;
        pNewList->pNext = nullptr;
        pNewList->pPrev = nullptr;

        NODE* pNode = pList;

        while( nullptr != pNode )
        {
            NODE* pCurrent = pNewList;
            NODE* pLast = nullptr;

            while( ( nullptr != pCurrent ) && ( pComparator->IsLessThan( pNode->pValue, pCurrent->pValue ) ) )
            {
                pLast = pCurrent;
                pCurrent = pCurrent->pPrev;
            }

            NODE* pNextNode = pNode->pNext;
            pNode->pNext = pLast;

            if( nullptr != pLast )
            {
                pLast->pPrev = pNode;
            }
            else
            {
                pNewList = pNode;
            }

            pNode->pPrev = pCurrent;

            if( nullptr != pCurrent )
            {
                pCurrent->pNext = pNode;
            }
            else
            {
                pStart = pNode;
            }

            pNode = pNextNode;
        }

        m_pHead = pStart;
        m_pTail = pNewList;
    }

    return( TRUE );
}

void CVPtrList::MoveToHead( LISTPOS pos )
{
    NODE *pNode = (NODE*)pos;

    if( pNode == m_pHead )
    {
        return;
    }

    // remove
    pNode->pPrev->pNext = pNode->pNext;
    if( pNode == m_pTail )
    {
        m_pTail = pNode->pPrev;
    }
    else
    {
        pNode->pNext->pPrev = pNode->pPrev;
    }

    // add
    pNode->pNext = m_pHead;
    pNode->pPrev = nullptr;
    m_pHead->pPrev = pNode;
    m_pHead = pNode;
}

void CVPtrList::MoveToTail( LISTPOS pos )
{
    NODE *pNode = (NODE*)pos;

    if( pNode == m_pTail )
    {
        return;
    }

    // remove
    if( pNode == m_pHead )
    {
        m_pHead = pNode->pNext;
    }
    else
    {
        pNode->pPrev->pNext = pNode->pNext;
    }
    pNode->pNext->pPrev = pNode->pPrev;

    // add
    pNode->pNext = nullptr;
    pNode->pPrev = m_pTail;
    m_pTail->pNext = pNode;
    m_pTail = pNode;
}

int CVPtrList::Compare( const void *pValue1, const void *pValue2 ) const
{
    return( !( pValue1 == pValue2 ) );
}

LISTPOS CVPtrList::GetHeadPosition() const
{
    return( (LISTPOS)m_pHead );
}

LISTPOS CVPtrList::GetTailPosition() const
{
    return( (LISTPOS)m_pTail );
}

BOOL CVPtrList::GetNext( LISTPOS& rPos, _Out_ void **ppNext ) const
{
    NODE *pNode = (NODE *)rPos;

    if( !pNode )
    {
        return( FALSE );
    }

    rPos = (LISTPOS)pNode->pNext;
    *ppNext = pNode->pValue;

    return( TRUE );
}

BOOL CVPtrList::GetPrev( LISTPOS& rPos, _Out_ void **ppPrev ) const
{
    NODE *pNode = (NODE *)rPos;

    if( !pNode )
    {
        return( FALSE );
    }

    rPos = (LISTPOS)pNode->pPrev;
    *ppPrev = pNode->pValue;

    return( TRUE );
}

BOOL CVPtrList::IsEmpty() const
{
    return( m_cEntries == 0 );
}

DWORD CVPtrList::GetCount() const
{
    return( m_cEntries );
}

BOOL CVPtrList::GetHead( __out void **ppHead ) const
{
    if( !ppHead || !m_pHead )
    {
        return( FALSE );
    }

    *ppHead = m_pHead->pValue;

    return( TRUE );
}

BOOL CVPtrList::GetTail( __out void **ppTail ) const
{
    if( !ppTail || !m_pTail )
    {
        return( FALSE );
    }

    *ppTail = m_pTail->pValue;

    return( TRUE );
}

BOOL CVPtrList::GetAt( LISTPOS pos, __out void **ppElement ) const
{
    NODE *pNode = (NODE *)pos;

    if( !pNode )
    {
        return( FALSE );
    }

    *ppElement = pNode->pValue;

    return( TRUE );
}

void CVPtrList::SetAt( LISTPOS pos, void* newElement )
{
    NODE *pNode = (NODE *)pos;
    if( !pNode )
    {
        return;
    }

    pNode->pValue = newElement;
}

