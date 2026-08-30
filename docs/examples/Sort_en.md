# Sorting Algorithms

[简体中文](Sort_zh.md) | English | [Project Home](../../README_en.md)

This document implements seven classic ascending sort algorithms in Tapas.
Each section introduces one algorithm and gives its Tapas implementation. The
last section runs all algorithms against the same input list and prints one
combined result.

The examples use two styles:

- Bubble, selection, insertion, shell, and heap sort mutate the list they
  receive.
- Merge sort and quick sort return a new sorted list.

The shared test at the end copies the original list before calling any
in-place algorithm, so the original input remains unchanged.

## Shared Helper

Several in-place algorithms need to exchange two elements. The `swap` helper
keeps that operation small and readable.

```tapas
var swap = (xs, i, j){
    let tmp = xs[i]
    xs[i] = xs[j]
    xs[j] = tmp
}
```



## Bubble Sort

Bubble sort repeatedly compares neighboring elements and swaps them when they
are in the wrong order. After each outer pass, one large element has moved to
the right side of the list.

Complexity: best `O(n)` if an early-exit optimization is added, but this simple
version is `O(n^2)` in all cases. Space is `O(1)`. Stability: stable, because it
only swaps adjacent values when the left value is strictly greater than the
right value.

```tapas
var bubble_sort = (xs){
    let n = xs.len()
    for(let i in 0 to n){
        for(let j in 0 to (n - i - 1)){
            if(xs[j] > xs[j + 1]){
                swap(xs, j, j + 1)
            }
        }
    }
    return xs
}
```



## Selection Sort

Selection sort scans the unsorted suffix, finds the smallest value, and moves
that value to the current position.

Complexity: `O(n^2)` comparisons in best, average, and worst cases. Space is
`O(1)`. Stability: not stable in this in-place swap version, because moving the
minimum value forward can cross equal elements.

```tapas
var selection_sort = (xs){
    let n = xs.len()
    for(let i in 0 to n){
        let min_idx = i
        for(let j in (i + 1) to n){
            if(xs[j] < xs[min_idx]){
                min_idx = j
            }
        }
        if(min_idx != i){
            swap(xs, i, min_idx)
        }
    }
    return xs
}
```



## Insertion Sort

Insertion sort grows a sorted prefix. Each new value is shifted left until it
lands after all smaller or equal values.

Complexity: `O(n)` best case on already sorted input, `O(n^2)` average and worst
case. Space is `O(1)`. Stability: stable, because equal values are not shifted
past each other.

```tapas
var insertion_sort = (xs){
    let n = xs.len()
    for(let i in 1 to n){
        let key = xs[i]
        let j = i - 1
        while(j >= 0 and xs[j] > key){
            xs[j + 1] = xs[j]
            j = j - 1
        }
        xs[j + 1] = key
    }
    return xs
}
```



## Shell Sort

Shell sort is a gapped insertion sort. It first moves values across larger
gaps, then reduces the gap until the final pass is ordinary insertion sort.

Complexity: depends on the gap sequence. With this halving sequence, worst-case
time is commonly treated as `O(n^2)`; space is `O(1)`. Stability: not stable,
because gapped moves can reorder equal values.

```tapas
var shell_sort = (xs){
    let n = xs.len()
    let gap = n / 2
    while(gap > 0){
        for(let i in gap to n){
            let temp = xs[i]
            let j = i
            while(j >= gap and xs[j - gap] > temp){
                xs[j] = xs[j - gap]
                j = j - gap
            }
            xs[j] = temp
        }
        gap = gap / 2
    }
    return xs
}
```



## Merge Sort

Merge sort splits the list, recursively sorts both halves, and then merges the
two sorted halves. Because Tapas functions are anonymous values, recursive
calls use `this(...)`.

Complexity: `O(n log n)` time in best, average, and worst cases. Space is
`O(n)` for the temporary lists. Stability: stable, because equal values are
taken from the left half before the right half.

```tapas
var merge_sort = (xs){
    if(xs.len() <= 1){
        return xs.copy()
    }

    let mid = xs.len() / 2
    let left = this(xs[:mid])
    let right = this(xs[mid:])
    let result = []
    let i = 0
    let j = 0

    while(i < left.len() and j < right.len()){
        if(left[i] <= right[j]){
            result.append(left[i])
            i = i + 1
        }
        else{
            result.append(right[j])
            j = j + 1
        }
    }
    while(i < left.len()){
        result.append(left[i])
        i = i + 1
    }
    while(j < right.len()){
        result.append(right[j])
        j = j + 1
    }

    return result.copy()
}
```



## Quick Sort

Quick sort partitions values around a pivot. This version keeps the code
friendly by building three lists: values smaller than the pivot, values equal
to the pivot, and values larger than the pivot.

Complexity: average `O(n log n)`, worst-case `O(n^2)` when pivots are badly
chosen. This functional version uses `O(n)` extra space at each partition level.
Stability: stable in this version, because values are appended to `left`,
`middle`, and `right` in their original order.

```tapas
var quick_sort = (xs){
    if(xs.len() <= 1){
        return xs.copy()
    }

    let pivot = xs[0]
    let left = []
    let middle = []
    let right = []

    for(let i in 0 to xs.len()){
        if(xs[i] < pivot){
            left.append(xs[i])
        }
        elif(xs[i] > pivot){
            right.append(xs[i])
        }
        else{
            middle.append(xs[i])
        }
    }

    return this(left).union(middle).union(this(right))
}
```



## Heap Sort

Heap sort first turns the list into a max heap. Then it repeatedly moves the
largest value to the end and repairs the heap.

The `heapify` helper checks whether a child index exists before reading
`xs[left]` or `xs[right]`. That keeps the example explicit and avoids accidental
out-of-range indexing.

Complexity: `O(n log n)` time in best, average, and worst cases. Space is
`O(1)`. Stability: not stable, because heap extraction swaps distant elements.

```tapas
var heapify = (xs, n, root){
    let current = root
    let done = false
    while(done == false){
        let largest = current
        let left = current * 2 + 1
        let right = current * 2 + 2

        if(left < n and xs[left] > xs[largest]){
            largest = left
        }
        if(right < n and xs[right] > xs[largest]){
            largest = right
        }
        if(largest != current){
            swap(xs, current, largest)
            current = largest
        }
        else{
            done = true
        }
    }
}

var heap_sort = (xs){
    let n = xs.len()
    let i = n / 2 - 1
    while(i >= 0){
        heapify(xs, n, i)
        i = i - 1
    }

    let end = n - 1
    while(end > 0){
        swap(xs, 0, end)
        heapify(xs, end, 0)
        end = end - 1
    }
    return xs
}
```



## Unified Test

This test uses the same list for every algorithm. The in-place algorithms sort
a copy, while merge sort and quick sort receive the original list and return a
new sorted list.

```tapas
var sample = [2, 7, 3, 4, 3, 1, 9, 1, 8, 4, 6, 9, 5]

var show_inplace_sort = (name, sorter){
    let xs = sample.copy()
    let sorted = sorter(xs)
    sprint(name, ': ', xs)
}

sprint('original: ', sample)
show_inplace_sort('bubble', bubble_sort)
show_inplace_sort('selection', selection_sort)
show_inplace_sort('insertion', insertion_sort)
show_inplace_sort('shell', shell_sort)
sprint('merge: ', merge_sort(sample))
sprint('quick: ', quick_sort(sample))
show_inplace_sort('heap', heap_sort)
sprint('original after sorting copies: ', sample)
```
<pre class='Tapas-Return'>
original: [2, 7, 3, 4, 3, 1, 9, 1, 8, 4, 6, 9, 5]
bubble: [1, 1, 2, 3, 3, 4, 4, 5, 6, 7, 8, 9, 9]
selection: [1, 1, 2, 3, 3, 4, 4, 5, 6, 7, 8, 9, 9]
insertion: [1, 1, 2, 3, 3, 4, 4, 5, 6, 7, 8, 9, 9]
shell: [1, 1, 2, 3, 3, 4, 4, 5, 6, 7, 8, 9, 9]
merge: [1, 1, 2, 3, 3, 4, 4, 5, 6, 7, 8, 9, 9]
quick: [1, 1, 2, 3, 3, 4, 4, 5, 6, 7, 8, 9, 9]
heap: [1, 1, 2, 3, 3, 4, 4, 5, 6, 7, 8, 9, 9]
original after sorting copies: [2, 7, 3, 4, 3, 1, 9, 1, 8, 4, 6, 9, 5]
</pre>
