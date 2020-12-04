#ifndef DKLIST_H
#define DKLIST_H

#define USE_QSORT

#define DEBUG_CHECK_LIST_BOUNDS

template< class T >
class DkList
{
public:
	DkList( int newgranularity = 16 );

	~DkList<T>();

	// cleans list
	void			clear( bool deallocate = true );

	// returns number of elements in list
	int				numElem( void ) const;

	// returns number elements which satisfies to the condition
	template< typename COMPAREFUNC >
	int				numElem( COMPAREFUNC comparator ) const;

	// returns number of elements allocated for
	int				numAllocated( void ) const;

	// sets new m_nGranularity
	void			setGranularity( int newgranularity );

	// get the current m_nGranularity
	int				getGranularity( void ) const;

	const T &		operator[]( int index ) const;
	T &				operator[]( int index );
	DkList<T> &		operator=( const DkList<T> &other );

	// resizes the list
	void			resize( int newsize );

	// sets the number of elements in list and resize to exactly this number if necessary
	void			setNum( int newnum, bool resize = true );

	// returns a pointer to the list
	T *				ptr();

	// returns a pointer to the list
	const T *		ptr() const;

	// appends element
	int				append( const T & obj );

	// appends another list
	int				append( const DkList<T> &other );

	// appends another array
	int				append( const T *other, int count );

	// appends another list with transformation
	// return false to not add the element
	template< class T2, typename TRANSFORMFUNC >
	int				append( const DkList<T2> &other, TRANSFORMFUNC transform );

	// inserts the element at the given index
	int				insert( const T & obj, int index = 0 );

	// adds unique element
	int				addUnique( const T & obj );

	// adds unique element
	int				addUnique( const T & obj, bool (* comparator )(const T &a, const T &b) );

	// finds the index for the given element
	int				findIndex( const T & obj ) const;

	// finds the index for the given element
	int				findIndex( const T & obj, bool (* comparator )(const T &a, const T &b) ) const;

	// finds pointer to the given element
	T *				find( T const & obj ) const;

	// returns first found element which satisfies to the condition
	template< typename COMPAREFUNC >
	T*				findFirst( COMPAREFUNC comparator ) const;

	// returns last found element which satisfies to the condition
	template< typename COMPAREFUNC >
	T*				findLast( COMPAREFUNC comparator ) const;

	// removes the element at the given index
	bool			removeIndex( int index );

	// removes the element at the given index (fast)
	bool			fastRemoveIndex( int index );

	// removes the element
	bool			remove( const T & obj );

	// removes the element
	bool			fastRemove( const T & obj );

	// returns true if index is in range
	bool			inRange( int index ) const;

	// swap the contents of the lists
	void			swap( DkList<T> &other );

	// assure list has given number of elements, but leave them uninitialized
	void			assureSize( int newSize);

	// assure list has given number of elements and initialize any new elements
	void			assureSize( int newSize, const T &initValue );

	// sorts list using quick sort algorithm
	void			sort(int ( * comparator ) ( const T &a, const T &b ));

	void			shellSort(int (* comparator )(const T &a, const T &b), int i0, int i1);
	void			quickSort(int (* comparator )(const T &a, const T &b), int p, int r);

protected:

	int				m_nNumElem;
	int				m_nSize;

	int				m_nGranularity;
	T *				m_pListPtr;
};

template< class T >
inline DkList<T>::DkList( int newgranularity )
{
	m_nNumElem		= 0;
	m_nSize			= 0;
	m_pListPtr		= NULL;
	m_nGranularity	= newgranularity;
}

template< class T >
inline DkList<T>::~DkList()
{
	delete [] m_pListPtr;
}

// -----------------------------------------------------------------
// Frees up the memory allocated by the list.  Assumes that T automatically handles freeing up memory.
// -----------------------------------------------------------------
template< class T >
inline void DkList<T>::clear(bool deallocate)
{
	if ( deallocate )
	{
		delete [] m_pListPtr;
		m_pListPtr	= NULL;
		m_nSize		= 0;
	}

	m_nNumElem		= 0;
}

// -----------------------------------------------------------------
// Returns the number of elements currently contained in the list.
// Note that this is NOT an indication of the memory allocated.
// -----------------------------------------------------------------
template< class T >
inline int DkList<T>::numElem( void ) const
{
	return m_nNumElem;
}

// -----------------------------------------------------------------
// returns number elements which satisfies to the condition
// -----------------------------------------------------------------
template< class T >
template< typename COMPAREFUNC >
inline int DkList<T>::numElem( COMPAREFUNC comparator ) const
{
	int theCount = 0;

	for(int i = 0; i < m_nNumElem; i++)
	{
		if(comparator(m_pListPtr[i]))
			theCount++;
	}

	return theCount;
}

// -----------------------------------------------------------------
// Returns the number of elements currently allocated for.
// -----------------------------------------------------------------
template< class T >
inline int DkList<T>::numAllocated( void ) const
{
	return m_nSize;
}

// -----------------------------------------------------------------
// Sets the base size of the array and resizes the array to match.
// -----------------------------------------------------------------
template< class T >
inline void DkList<T>::setGranularity( int newgranularity )
{
	int newsize;

	ASSERT( newgranularity > 0);

	m_nGranularity = newgranularity;

	if ( m_pListPtr )
	{
		// resize it to the closest level of granularity
		newsize = m_nNumElem + m_nGranularity - 1;
		newsize -= newsize % m_nGranularity;

		if ( newsize != m_nSize )
		{
			resize( newsize );
		}
	}
}

// -----------------------------------------------------------------
// Returns the current granularity.
// -----------------------------------------------------------------
template< class T >
inline int DkList<T>::getGranularity( void ) const
{
	return m_nGranularity;
}

// -----------------------------------------------------------------
// Access operator.  Index must be within range or an assert will be issued in debug builds.
// Release builds do no range checking.
// -----------------------------------------------------------------
template< class T >
inline const T &DkList<T>::operator[]( int index ) const
{
	return m_pListPtr[ index ];
}

// -----------------------------------------------------------------
// Access operator.  Index must be within range or an assert will be issued in debug builds.
// Release builds do no range checking.
// -----------------------------------------------------------------
template< class T >
inline T &DkList<T>::operator[]( int index )
{
	return m_pListPtr[ index ];
}

// -----------------------------------------------------------------
// Copies the contents and size attributes of another list.
// -----------------------------------------------------------------
template< class T >
inline DkList<T> &DkList<T>::operator=( const DkList<T> &other )
{
	m_nGranularity	= other.m_nGranularity;

	assureSize(other.m_nSize);

	m_nNumElem = other.m_nNumElem;

	if (m_nSize)
	{
		for( int i = 0; i < m_nNumElem; i++ )
			m_pListPtr[i] = other.m_pListPtr[i];
	}

	return *this;
}



// -----------------------------------------------------------------
// Allocates memory for the amount of elements requested while keeping the contents intact.
// Contents are copied using their = operator so that data is correnctly instantiated.
// -----------------------------------------------------------------
template< class T >
inline void DkList<T>::resize( int newsize )
{
	T	*temp;
	int		i;

	// free up the m_pListPtr if no data is being reserved
	if ( newsize <= 0 )
	{
		clear();
		return;
	}

	// not changing the elemCount, so just exit
	if ( newsize == m_nSize )
		return;

	temp	= m_pListPtr;
	m_nSize	= newsize;

	if ( m_nSize < m_nNumElem )
		m_nNumElem = m_nSize;

	// copy the old m_pListPtr into our new one
	m_pListPtr = new T[ m_nSize ];

	if(temp)
	{
		for( i = 0; i < m_nNumElem; i++ )
			m_pListPtr[ i ] = temp[ i ];

		// delete the old m_pListPtr if it exists
		delete[] temp;
	}
}

// -----------------------------------------------------------------
// Resize to the exact size specified irregardless of granularity
// -----------------------------------------------------------------
template< class T >
inline void DkList<T>::setNum( int newnum, bool bResize )
{
	if ( bResize || newnum > m_nSize )
		resize( newnum );

	m_nNumElem = newnum;
}

// -----------------------------------------------------------------
// Returns a pointer to the begining of the array.  Useful for iterating through the list in loops.
// Note: may return NULL if the list is empty.
// -----------------------------------------------------------------

template< class T >
inline T *DkList<T>::ptr( void )
{
	return m_pListPtr;
}

// -----------------------------------------------------------------
// Returns a pointer to the begining of the array.  Useful for iterating through the list in loops.
// Note: may return NULL if the list is empty.
// -----------------------------------------------------------------
template< class T >
inline const T *DkList<T>::ptr( void ) const
{
	return m_pListPtr;
}

// -----------------------------------------------------------------
// Increases the size of the list by one element and copies the supplied data into it.
// Returns the index of the new element.
// -----------------------------------------------------------------
template< class T >
inline int DkList<T>::append( T const & obj )
{
	if ( !m_pListPtr )
		resize(m_nGranularity);

	if ( m_nNumElem == m_nSize )
	{
		int newsize;

		if ( m_nGranularity == 0 )	// this is a hack to fix our memset classes
			m_nGranularity = 16;

		newsize = m_nSize + m_nGranularity;
		resize( newsize - newsize % m_nGranularity );
	}

	m_pListPtr[m_nNumElem] = obj;
	m_nNumElem++;

	return m_nNumElem - 1;
}

// -----------------------------------------------------------------
// adds the other list to this one
// Returns the size of the new combined list
// -----------------------------------------------------------------

template< class T >
inline int DkList<T>::append( const DkList<T> &other )
{
	int nOtherElems = other.numElem();

	if ( !m_pListPtr )
	{
		if ( m_nGranularity == 0 )	// this is a hack to fix our memset classes
			m_nGranularity = 16;

		// pre-allocate for a bigger list to do less memory resize access
		int newSize = nOtherElems+m_nGranularity;

		resize( newSize - (newSize % m_nGranularity) );
	}
	else
	{
		// pre-allocate for a bigger list to do less memory resize access
		if ( m_nNumElem+nOtherElems >= m_nSize )
		{
			int newSize = nOtherElems+m_nNumElem+m_nGranularity;

			resize( newSize - (newSize % m_nGranularity) );
		}
	}

	// append the elements
	for (int i = 0; i < nOtherElems; i++)
		append(other[i]);

	return numElem();
}

// -----------------------------------------------------------------
// adds the other list to this one
// Returns the size of the new combined list
// -----------------------------------------------------------------
template< class T >
inline int DkList<T>::append( const T *other, int count )
{
	if ( !m_pListPtr )
	{
		if ( m_nGranularity == 0 )	// this is a hack to fix our memset classes
			m_nGranularity = 16;

		// pre-allocate for a bigger list to do less memory resize access
		int newSize = count+m_nGranularity;

		resize( newSize - (newSize % m_nGranularity) );
	}
	else
	{
		// pre-allocate for a bigger list to do less memory resize access
		if ( m_nNumElem+count >= m_nSize )
		{
			int newSize = count+m_nNumElem+m_nGranularity;

			resize( newSize - (newSize % m_nGranularity) );
		}
	}

	// append the elements
	for (int i = 0; i < count; i++)
		append(other[i]);

	return numElem();
}

// -----------------------------------------------------------------
// appends another list with transformation
// return false to not add the element
// -----------------------------------------------------------------
template< class T >
template< class T2, typename TRANSFORMFUNC >
inline int DkList<T>::append( const DkList<T2> &other, TRANSFORMFUNC transform )
{
	int nOtherElems = other.numElem();

	// it can't predict how many elements it will have. So this is slower implementation

	// try transform and append the elements
	for (int i = 0; i < nOtherElems; i++)
	{
		T newObj;
		if(transform(newObj, other[i]))
			append(other[i]);
	}


	return numElem();
}

// -----------------------------------------------------------------
// Increases the elemCount of the list by at leat one element if necessary
// and inserts the supplied data into it.
// -----------------------------------------------------------------
template< class T >
inline int DkList<T>::insert( T const & obj, int index )
{
	if ( !m_pListPtr )
		resize( m_nGranularity );

	if ( m_nNumElem == m_nSize )
	{
		int newsize;

		if ( m_nGranularity == 0 )	// this is a hack to fix our memset classes
			m_nGranularity = 16;

		newsize = m_nSize + m_nGranularity;
		resize( newsize - newsize % m_nGranularity );
	}

	if ( index < 0 )
		index = 0;
	else if ( index > m_nNumElem )
		index = m_nNumElem;
	for ( int i = m_nNumElem; i > index; --i )
		m_pListPtr[i] = m_pListPtr[i-1];

	m_nNumElem++;
	m_pListPtr[index] = obj;

	return index;
}

// -----------------------------------------------------------------
// Adds the data to the m_pListPtr if it doesn't already exist.  Returns the index of the data in the m_pListPtr.
// -----------------------------------------------------------------

template< class T >
inline int DkList<T>::addUnique( T const & obj )
{
	int index;

	index = findIndex( obj );

	if ( index < 0 )
		index = append( obj );

	return index;
}


// -----------------------------------------------------------------
// Adds the data to the m_pListPtr if it doesn't already exist.  Returns the index of the data in the m_pListPtr.
// -----------------------------------------------------------------

template< class T >
inline int DkList<T>::addUnique( T const & obj, bool (* comparator )(const T &a, const T &b) )
{
	int index;

	index = findIndex( obj, comparator );

	if ( index < 0 )
		index = append( obj );

	return index;
}

// -----------------------------------------------------------------
// Searches for the specified data in the m_pListPtr and returns it's index.
// Returns -1 if the data is not found.
// -----------------------------------------------------------------

template< class T >
inline int DkList<T>::findIndex( T const & obj ) const
{
	int i;

	for( i = 0; i < m_nNumElem; i++ )
	{
		if ( m_pListPtr[ i ] == obj )
			return i;
	}

	// Not found
	return -1;
}

// -----------------------------------------------------------------
// Searches for the specified data in the m_pListPtr and returns it's index.
// Returns -1 if the data is not found.
// -----------------------------------------------------------------

template< class T >
inline int DkList<T>::findIndex( T const & obj, bool (* comparator )(const T &a, const T &b) ) const
{
	int i;

	for( i = 0; i < m_nNumElem; i++ )
	{
		if ( comparator(m_pListPtr[ i ], obj) )
			return i;
	}

	// Not found
	return -1;
}

// -----------------------------------------------------------------
// Searches for the specified data in the m_pListPtr and returns it's address.
// Returns NULL if the data is not found.
// -----------------------------------------------------------------

template< class T >
inline T *DkList<T>::find( T const & obj ) const
{
	int i;

	i = findIndex( obj );

	if ( i >= 0 )
		return &m_pListPtr[ i ];

	return NULL;
}

// -----------------------------------------------------------------
// returns first element which satisfies to the condition
// Returns NULL if the data is not found.
// -----------------------------------------------------------------
template< class T >
template< typename COMPAREFUNC >
inline T *DkList<T>::findFirst( COMPAREFUNC comparator  ) const
{
	for( int i = 0; i < m_nNumElem; i++ )
	{
		if ( comparator(m_pListPtr[i]) )
			return &m_pListPtr[i];
	}

	return NULL;
}

// -----------------------------------------------------------------
// returns last element which satisfies to the condition
// Returns NULL if the data is not found.
// -----------------------------------------------------------------
template< class T >
template< typename COMPAREFUNC >
inline T *DkList<T>::findLast( COMPAREFUNC comparator ) const
{
	for( int i = m_nNumElem-1; i >= 0; i-- )
	{
		if ( comparator(m_pListPtr[i]) )
			return &m_pListPtr[i];
	}

	return NULL;
}

// -----------------------------------------------------------------
// Removes the element at the specified index and moves all data following the element down to fill in the gap.
// The number of elements in the m_pListPtr is reduced by one.  Returns false if the index is outside the bounds of the m_pListPtr.
// Note that the element is not destroyed, so any memory used by it may not be freed until the destruction of the m_pListPtr.
// -----------------------------------------------------------------
template< class T >
inline bool DkList<T>::removeIndex( int index )
{
	int i;

#ifdef DEBUG_CHECK_LIST_BOUNDS
	ASSERT( m_pListPtr != NULL );
	ASSERT( index >= 0 );
	ASSERT( index < m_nNumElem );
#endif // DEBUG_CHECK_LIST_BOUNDS

	if (( index < 0 ) || ( index >= m_nNumElem ))
		return false;

	m_nNumElem--;

	for( i = index; i < m_nNumElem; i++ )
		m_pListPtr[ i ] = m_pListPtr[ i + 1 ];

	return true;
}

// -----------------------------------------------------------------
// Removes the element at the specified index, without data movement (only last element will be placed at the removed element's position)
// The number of elements in the m_pListPtr is reduced by one.  Returns false if the index is outside the bounds of the m_pListPtr.
// Note that the element is not destroyed, so any memory used by it may not be freed until the destruction of the m_pListPtr.
// -----------------------------------------------------------------
template< class T >
inline bool DkList<T>::fastRemoveIndex( int index )
{
#ifdef DEBUG_CHECK_LIST_BOUNDS
	ASSERT( m_pListPtr != NULL );
	ASSERT( index >= 0 );
	ASSERT( index < m_nNumElem );
#endif // DEBUG_CHECK_LIST_BOUNDS

	if (( index < 0 ) || ( index >= m_nNumElem ))
		return false;

	m_nNumElem--;

	if( m_nNumElem > 0 )
		m_pListPtr[ index ] = m_pListPtr[ m_nNumElem ];

	return true;
}


// -----------------------------------------------------------------
// Removes the element if it is found within the m_pListPtr and moves all data following the element down to fill in the gap.
// The number of elements in the m_pListPtr is reduced by one.  Returns false if the data is not found in the m_pListPtr.  Note that
// the element is not destroyed, so any memory used by it may not be freed until the destruction of the m_pListPtr.
// -----------------------------------------------------------------
template< class T >
inline bool DkList<T>::remove( T const & obj )
{
	int index;

	index = findIndex( obj );

	if (index >= 0)
		return removeIndex( index );

	return false;
}


// -----------------------------------------------------------------
// Removes the element if it is found within the m_pListPtr without data movement (only last element will be placed at the removed element's position)
// The number of elements in the m_pListPtr is reduced by one.  Returns false if the data is not found in the m_pListPtr.  Note that
// the element is not destroyed, so any memory used by it may not be freed until the destruction of the m_pListPtr.
// -----------------------------------------------------------------
template< class T >
inline bool DkList<T>::fastRemove( T const & obj )
{
	int index;

	index = findIndex( obj );

	if (index >= 0)
		return fastRemoveIndex( index );

	return false;
}

// -----------------------------------------------------------------
// Returns true if index is in array range
// -----------------------------------------------------------------
template< class T >
inline bool DkList<T>::inRange( int index ) const
{
	return index >= 0 && index < m_nNumElem;
}

// -----------------------------------------------------------------
// Swaps the contents of two lists
// -----------------------------------------------------------------
template< class T >
inline void DkList<T>::swap( DkList<T> &other )
{
	QuickSwap( m_nNumElem, other.m_nNumElem );
	QuickSwap( m_nSize, other.m_nSize );
	QuickSwap( m_nGranularity, other.m_nGranularity );
	QuickSwap( m_pListPtr, other.m_pListPtr );
}

// -----------------------------------------------------------------
// Makes sure the list has at least the given number of elements.
// -----------------------------------------------------------------

template< class T >
inline void DkList<T>::assureSize( int newSize )
{
	int newNum = newSize;

	if ( newSize > m_nSize )
	{
		if ( m_nGranularity == 0 )	// this is a hack to fix our memset classes
			m_nGranularity = 16;

		newSize += m_nGranularity - 1;
		newSize -= newSize % m_nGranularity;

		resize( newSize );
	}

	m_nNumElem = newNum;
}

// -----------------------------------------------------------------
// Makes sure the m_pListPtr has at least the given number of elements and initialize any elements not yet initialized.
// -----------------------------------------------------------------
template< class T >
inline void DkList<T>::assureSize( int newSize, const T &initValue )
{
	int newNum = newSize;

	if ( newSize > m_nSize )
	{
		if ( m_nGranularity == 0 )	// this is a hack to fix our memset classes
			m_nGranularity = 16;

		newSize += m_nGranularity - 1;
		newSize -= newSize % m_nGranularity;

		m_nNumElem = m_nSize;

		resize( newSize );

		for ( int i = m_nNumElem; i < newSize; i++ )
			m_pListPtr[i] = initValue;
	}

	m_nNumElem = newNum;
}

// -----------------------------------------------------------------
// Sorts array of the elements by the comparator
// -----------------------------------------------------------------

template< class T >
inline void DkList<T>::sort(int ( * comparator ) ( const T &a, const T &b))
{
#ifdef USE_QSORT
	quickSort(comparator, 0, m_nNumElem - 1);
#else
	shellSort(comparator, 0, m_nNumElem - 1);
#endif
}

// -----------------------------------------------------------------
// Shell sort
// -----------------------------------------------------------------
template< class T >
inline void DkList<T>::shellSort(int (* comparator )(const T &a, const T &b), int i0, int i1)
{
	const int SHELLJMP = 3; //2 or 3

	int n = i1-i0;

	int gap;

	for(gap = 1; gap < n; gap = gap*SHELLJMP+1);

	for(gap = int(gap/SHELLJMP); gap > 0; gap = int(gap/SHELLJMP))
	{
		for( int i = i0; i < i1-gap; i++)
		{
			for( int j = i; (j >= i0) && (comparator)(m_pListPtr[j], m_pListPtr[j + gap]); j -= gap )
			{
				// T t = a[j];

				QuickSwap(m_pListPtr[j], m_pListPtr[j + gap]);

				// a[j] = a[j + gap];
				// a[j + gap] = t;
			}
		}
	}
}

template< class T >
inline int partition(T* list, int(*comparator)(const T &elem0, const T &elem1), int p, int r);

// -----------------------------------------------------------------
// Partition exchange sort
// -----------------------------------------------------------------
template< class T >
inline void DkList<T>::quickSort(int(*comparator)(const T &elem0, const T &elem1), int p, int r)
{
	if (p < r)
	{
		int q = partition(m_pListPtr, comparator, p, r);

		quickSort(comparator, p, q - 1);
		quickSort(comparator, q + 1, r);
	}
}

// -----------------------------------------------------------------
// Partition exchange sort
// -----------------------------------------------------------------
template< class T >
inline int partition(T* list, int (* comparator )(const T &elem0, const T &elem1), int p, int r)
{
	T tmp, pivot = list[p];
	int left = p;

	for (int i = p + 1; i <= r; i++)
	{
		if (comparator(list[i], pivot) < 0)
		{
			left++;
			tmp = list[i];
			list[i] = list[left];
			list[left] = tmp;
		}
	}

	tmp = list[p];
	list[p] = list[left];
	list[left] = tmp;

	return left;
}

// -----------------------------------------------------------------
// Partition exchange sort
// -----------------------------------------------------------------
template< class T >
inline void quickSort(T* list, int(*comparator)(const T &elem0, const T &elem1), int p, int r)
{
	if (p < r)
	{
		int q = partition(list, comparator, p, r);

		quickSort(list, comparator, p, q - 1);
		quickSort(list, comparator, q + 1, r);
	}
}

template< class T >
inline void shellSort(T* list, int numElems, int (* comparator )(const T &elem0, const T &elem1))
{
	const int SHELLJMP = 3; //2 or 3

	int i0 = 0;
	int i1 = numElems-1;

	int n = i1-i0;

	int gap;

	for(gap = 1; gap < n; gap = gap*SHELLJMP+1);

	for(gap = int(gap/SHELLJMP); gap > 0; gap = int(gap/SHELLJMP))
	{
		for( int i = i0; i < i1-gap; i++)
		{
			for( int j = i; (j >= i0) && (comparator)(list[j], list[j + gap]); j -= gap )
			{
				// T t = a[j];

				QuickSwap(list[j], list[j + gap]);

				// a[j] = a[j + gap];
				// a[j + gap] = t;
			}
		}
	}
}

#endif //DKLIST_H
