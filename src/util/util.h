#ifndef UTIL_H
#define UTIL_H
#include <vector>

char* varargs(const char* fmt, ...);

// -----------------------------------------------------------------
// Partition exchange sort
// -----------------------------------------------------------------
template< class T >
int partition(std::vector<T>& list, int (* comparator )(const T &elem0, const T &elem1), int p, int r)
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
// Shell sort
// -----------------------------------------------------------------
template< class T >
void shellSort(std::vector<T>& list, int (* comparator )(const T &a, const T &b), int i0, int i1)
{
	const int SHELLJMP = 3; //2 or 3

	int n = i1-i0;

	int gap;

	for(gap = 1; gap < n; gap = gap*SHELLJMP+1);

	for(gap = int(gap/SHELLJMP); gap > 0; gap = int(gap/SHELLJMP))
	{
		for( int i = i0; i < i1-gap; i++)
		{
			for( int j = i; (j >= i0) && (comparator)(list[j], list[j + gap]); j -= gap )
			{
				T t = a[j];
				a[j] = a[j + gap];
				a[j + gap] = t;
			}
		}
	}
}

template< class T >
int partition(T* list, int(*comparator)(const T &elem0, const T &elem1), int p, int r);

// -----------------------------------------------------------------
// Partition exchange sort
// -----------------------------------------------------------------
template< class T >
void quickSort(std::vector<T>& list, int(*comparator)(const T &elem0, const T &elem1), int p, int r)
{
	if (p < r)
	{
		int q = partition(list, comparator, p, r);

		quickSort(list, comparator, p, q - 1);
		quickSort(list, comparator, q + 1, r);
	}
}


#endif // UTIL_H