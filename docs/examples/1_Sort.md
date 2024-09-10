---
layout: post
title: "Tapas Programming Language"
use_math: false
---



# 3.1. Sort

The first notice is about passing parameters of functions. If you pass an object of composite type into function, then any changes on this parameter would also affect the outer object. This is because parameters passing follows the shallow copy rule with only the reference counter of the object increases by one and its entity (in a heap block) not copied. List sorting is a good example to illustrate this notice.

First, let's define a list

```tapas
let arr : list = [2, 7, 3, 4, 3, 1, 9, 1, 8, 4, 6, 9, 5]
```

Then, let's display this list

```tapas
arr
```
<pre class='Tapas-Return'>
[2, 7, 3, 4, 3, 1, 9, 1, 8, 4, 6, 9, 5]
</pre>
<br>

## 3.1.1. Bubble sorting

The bubble sorting algorithm is implemented below:

```tapas
let bubble_sort: fn = (arr) {
	for (let i: any in std::toiter(arr.std::len(), -1, 0)) {
		for (let j: any in 0 to i - 1) {
			if (arr[j] > arr[j + 1]) {
				let mid: any = arr[j + 1]
				arr[j + 1] = arr[j]
				arr[j] = mid
			}
		}
	}
}
```

Now let's call this function to sort the list ``arr`` that we defined above:

```tapas
let arr_copy: list = arr.std::copy()   // first, get a copy of our list
arr_copy.bubble_sort()             // sort the copied list
arr_copy                                          // display the copied list after sorting
```
<pre class='Tapas-Return'>
[1, 1, 2, 3, 3, 4, 4, 5, 6, 7, 8, 9, 9]
</pre>
<br>

## 3.1.2. Quick sorting

```tapas
let quick_sort: fn = (arr) {
	if (arr.std::len() <= 1) {
		return
	}
	let mididx: any = (arr.std::len() / 2 - 1).std::toint()
	let mid: any = arr[mididx]
	arr.std::delete(mididx) // delete the mid from arr
	let arr_l: any = []
	let arr_r: any = []
	for (let e: any in arr) {
		if (e <= mid) {
			arr_l.std::append(e)
		}
		else{
			arr_r.std::append(e)
		}
	}
	arr.std::insert(mid, mididx) // restore mid into arr
	this(arr_l)
	this(arr_r)
	arr[0 : arr_l.std::len()] = arr_l
	arr[arr_l.std::len()] = mid
	arr[arr_l.std::len() + 1 : arr.std::len()] = arr_r
}
```

Now let's call the quick sorting:

```tapas
arr_copy = arr.std::copy()       // first, get a copy of our list
arr_copy.quick_sort()            // sort the copied list
arr_copy                         // display the copied list after sorting
```
<pre class='Tapas-Return'>
[1, 1, 2, 3, 3, 4, 4, 5, 6, 7, 8, 9, 9]
</pre>
<br>

## 3.1.3. Performance Comparison

```tapas
arr_copy = arr.std::copy()
arr_copy = arr_copy.std::union(arr_copy).std::union(arr_copy).std::union(arr_copy)
arr_copy = arr_copy.std::union(arr_copy).std::union(arr_copy).std::union(arr_copy)
arr_copy = arr_copy.std::union(arr_copy).std::union(arr_copy).std::union(arr_copy)
arr_copy = arr_copy.std::union(arr_copy).std::union(arr_copy)
arr_copy.std::len()
```
<pre class='Tapas-Return'>
2496
</pre>

Firstly, we test bubble sorting algorithm:
```tapas
let arr1: list = arr_copy.std::copy()
let time_begin: time = std::now();
arr1.bubble_sort();
std::print(std::now() - time_begin, ' seconds')
```
<pre class='Tapas-Return'>
2.000000 seconds
</pre>

Nextly, we test quick sorting algorithm:
```tapas
let arr2: list = arr_copy.std::copy()
time_begin = std::now();
arr2.quick_sort();
std::print(std::now() - time_begin, ' seconds')
```
<pre class='Tapas-Return'>
0.000000 seconds
</pre>
In the above example, we use the keyword ``this`` for function environment copy.

Since function is an executable environment, the ``this`` algorithm actually supported for the recursion.
