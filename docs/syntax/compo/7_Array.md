---
layout: post
title: "Tapas Programming Language"
use_math: false
---



# 1.6.7. Array

We use C++ ``eigen`` package for the implementation of 2 dimensional array. (n dimensional tensors are
not supported in yet). Tap support boolean array and double array.

<br>

## 1.6.7.1. Creation

Array can be created by the function ``eig::toarr(rows, cols, value)``. For example, you can set all elements of your array to be the same value:

```
let ones = eig::toarr(3, 3, 1)
ones
```
<pre class='Tapas-Return'>
1 1 1
1 1 1
1 1 1
</pre>
```
let trues = eig::toarr(2, 3, true)
trues
```
<pre class='Tapas-Return'>
1 1 1
1 1 1
</pre>

or you can transform a list into an array

```
let seqs = eig::toarr(2, 3, [1, 2, 3, 6, 5, 4])
seqs
```
<pre class='Tapas-Return'>
1 2 3
6 5 4
</pre>
```
let booleans = eig::toarr(2, 3, [true, false, false, true, true, false])
booleans
```
<pre class='Tapas-Return'>
1 0 0
1 1 0
</pre>

For double array, integers would be transformed into double floats. For example,

```
seqs[0, 0]
```
<pre class='Tapas-Return'>
1.000000
</pre>
You can also use the function ``eig::random(rows, cols)`` to create a random matrix

```
let rad = eig::random(3, 3)
rad
```
<pre class='Tapas-Return'>
7.82637e-06     0.45865   0.0470446
   0.131538    0.532767    0.678865
   0.755605    0.218959    0.679296
</pre>
All elements of the random matrix follows uniform distribution in $[0, 1]$.

<br>

## 1.6.7.2. Indexing

Array is indexable. You can get the elements an by indexing. Array supports value indexing and slice indexing.

```
seqs[1, 1]
seqs[1, 0:3] // the second row and columns 0, 1, 2
seqs[0:2, 1] // rows 0, 1 and the second column
seqs[0:2, 1:3]
```
<pre class='Tapas-Return'>
5.000000
6 5 4
2
5
2 3
5 4
</pre>
Also, you can set the elements of array by indexing

```
seqs[0, 0] = 0
seqs[0, 1:3] = eig::toarr(1, 2, [3, 5])
seqs[0:2, 0] = eig::toarr(2, 1, [1, 2])
seqs[1:2, 1:3] = eig::toarr(1, 2, [4, 6])
seqs
```
<pre class='Tapas-Return'>
1 3 5
2 4 6
</pre>
<br>

## 1.6.7.3. Transpose and Pick Sub Array

Next, we will show the basic array transposing and sub array picking methods. Matrix transformation:

```
rad.eig::t()
```
<pre class='Tapas-Return'>
7.82637e-06    0.131538    0.755605
    0.45865    0.532767    0.218959
  0.0470446    0.678865    0.679296
</pre>
Use the function ``eig::top(n)`` to pick the first n rows of array as a sub array

```
rad.eig::top(2)
```
<pre class='Tapas-Return'>
7.82637e-06     0.45865   0.0470446
   0.131538    0.532767    0.678865
</pre>
Use the function ``eig::bottom(n)`` to pick the last n rows of array as a sub array

```
rad.eig::left(2)
```
<pre class='Tapas-Return'>
7.82637e-06     0.45865
   0.131538    0.532767
   0.755605    0.218959
</pre>
Use the function ``eig::left(n)`` to pick the left n columns of array as a sub array

```
rad.eig::left(2)
```
<pre class='Tapas-Return'>
7.82637e-06     0.45865
   0.131538    0.532767
   0.755605    0.218959
</pre>
Use the function ``eig::right(n)`` to pick the right n columns of array as a sub array

```
rad.eig::right(2)
```
<pre class='Tapas-Return'>
  0.45865 0.0470446
 0.532767  0.678865
 0.218959  0.679296
</pre>
Use the function ``eig::topleft(m, n)`` to pick the sub array of top-left corner by m rows and n columns

```
rad.eig::topleft(2, 2)
```
<pre class='Tapas-Return'>
7.82637e-06     0.45865
   0.131538    0.532767
</pre>

Use the function ``eig::topright(m, n)`` to pick the sub array of top-right corner by m rows and n columns

```
rad.eig::topright(2, 2)
```
<pre class='Tapas-Return'>
  0.45865 0.0470446
 0.532767  0.678865
</pre>
Use the function ``eig::bottomleft(m, n)`` to pick the sub array of bottom-left corner by m rows and n columns

```
rad.eig::bottomleft(2, 2)
```
<pre class='Tapas-Return'>
0.131538 0.532767
0.755605 0.218959
</pre>

Use the function ``eig::bottomright(m, n)`` to pick the sub array of bottom-right corner by m rows and n columns

```
rad.eig::bottomright(2, 2)
```
<pre class='Tapas-Return'>
0.532767 0.678865
0.218959 0.679296
</pre>
<br>

## 1.6.7.4. Operators for double array

In this section we will show the supported operators for double array. We first create two random matrix

```
let rad1 = eig::random(3, 3)
let rad2 = eig::random(3, 3)
```

Addition

```
rad1 + rad2
```
<pre class='Tapas-Return'>
  1.31811   1.51774   1.37587
 0.450344  0.623549   1.19808
 0.936902  0.983898 0.0996631
</pre>

Subtraction

```
rad1 - rad2
```
<pre class='Tapas-Return'>
  0.551277   0.144193  -0.316467
   0.31666  -0.554405   0.144221
   0.10193  -0.876975 -0.0842667
</pre>

Multiplication

```
rad1 * rad2
```
<pre class='Tapas-Return'>
   0.358376    0.570684    0.448215
  0.0256341   0.0203622    0.353648
   0.216849   0.0497427 0.000707963
</pre>

Division

```
rad1 / rad2
```
<pre class='Tapas-Return'>
  2.43781   1.20996     0.626
  5.73742 0.0586986    1.2737
  1.24415 0.0574587 0.0837079
</pre>

Power

```
rad1 ^ rad2
```
<pre class='Tapas-Return'>
 0.974438  0.880586  0.584095
 0.937947   0.13783  0.810487
 0.760732 0.0655427  0.639178
</pre>

Matrix multiplication

```
rad1 @ rad2
```
<pre class='Tapas-Return'>
0.635062  1.62419  1.27748
0.429547 0.908203 0.404446
 0.20594 0.395371 0.468391
</pre>
Greater

```
rad1 > rad2
```
<pre class='Tapas-Return'>
1 1 0
1 0 1
1 0 0
</pre>

Less

```
rad1 < rad2
```
<pre class='Tapas-Return'>
0 0 1
0 1 0
0 1 1
</pre>

Equal

```
rad1 == rad2
```
<pre class='Tapas-Return'>
0 0 0
0 0 0
0 0 0
</pre>

Not equal

```
rad1 != rad2
```
<pre class='Tapas-Return'>
1 1 1
1 1 1
1 1 1
</pre>

Greater or equal to

```
rad1 >= rad2
```
<pre class='Tapas-Return'>
1 1 0
1 0 1
1 0 0
</pre>

Less or equal to

```
rad1 <= rad2
```
<pre class='Tapas-Return'>
0 0 1
0 1 0
0 1 1
</pre>
<br>

## 1.6.7.5. Operators for boolean array

Operator and

```
rad1 > rad2 and rad1 <= rad2
```
<pre class='Tapas-Return'>
0 0 0
0 0 0
0 0 0
</pre>

Operator or

```
rad1 > rad2 or rad1 <= rad2
```
<pre class='Tapas-Return'>
1 1 1
1 1 1
1 1 1
</pre>
