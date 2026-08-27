# 排序算法

简体中文 | [English](Sort_en.md)

本文使用 Tapas 实现七种经典的升序排序算法。每一节介绍一种算法并给出相应的
Tapas 实现，最后一节使用同一输入列表运行全部算法并集中输出结果。

示例采用两种处理方式：

- 冒泡、选择、插入、希尔和堆排序会直接修改传入的列表。
- 归并排序和快速排序会返回一个新的有序列表。

末尾的统一测试会在调用原地排序算法前复制原列表，因此原始输入不会改变。

## 公共辅助函数

多个原地排序算法都需要交换两个元素。辅助函数 `swap` 让这项操作保持简洁、
易读。

```tapas
var swap = (xs, i, j){
    let tmp = xs[i]
    xs[i] = xs[j]
    xs[j] = tmp
}
```



## 冒泡排序

冒泡排序反复比较相邻元素，并在顺序错误时交换它们。每完成一轮外层循环，
就会有一个较大的元素移动到列表右侧。

复杂度：加入提前退出优化后最好为 `O(n)`，但这里的简单实现始终为 `O(n^2)`。
空间复杂度为 `O(1)`。该算法稳定，因为只有左值严格大于右值时才交换相邻元素。

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



## 选择排序

选择排序扫描列表中尚未排好序的部分，找到最小值，再将它移动到当前位置。

复杂度：最好、平均和最坏情况都需要 `O(n^2)` 次比较；空间复杂度为 `O(1)`。
这个原地交换版本不稳定，因为向前移动最小值时可能越过相等元素。

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



## 插入排序

插入排序逐步扩展已有序的前缀。每个新值不断向左移动，直到落在所有小于或
等于它的值之后。

复杂度：输入已有序时最好为 `O(n)`，平均和最坏情况为 `O(n^2)`；空间复杂度
为 `O(1)`。该算法稳定，因为相等元素不会越过彼此。

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



## 希尔排序

希尔排序是带间隔的插入排序。它先让元素跨越较大的间隔移动，再逐步缩小间隔，
最后一轮退化为普通插入排序。

复杂度取决于间隔序列。采用这里的折半序列时，最坏时间复杂度通常按 `O(n^2)`
计算；空间复杂度为 `O(1)`。该算法不稳定，因为跨间隔移动可能改变相等元素的
先后顺序。

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



## 归并排序

归并排序将列表拆成两半，分别递归排序，再合并两个已有序的部分。由于 Tapas
函数是匿名值，递归调用使用 `this(...)`。

最好、平均和最坏时间复杂度均为 `O(n log n)`；临时列表需要 `O(n)` 空间。
该算法稳定，因为相等元素会优先从左半部分取出。

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



## 快速排序

快速排序围绕基准值对元素进行分区。为使代码更清晰，这个版本建立三个列表，
分别保存小于、等于和大于基准值的元素。

平均时间复杂度为 `O(n log n)`；基准选择不佳时最坏为 `O(n^2)`。这个函数式
版本在每层分区中使用 `O(n)` 额外空间。它是稳定的，因为元素会按原始顺序
追加到 `left`、`middle` 和 `right`。

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



## 堆排序

堆排序先将列表构造成最大堆，然后反复把最大值移动到末尾并修复堆。

辅助函数 `heapify` 会在读取 `xs[left]` 或 `xs[right]` 前检查子节点下标是否
存在，使边界处理一目了然，并避免意外越界。

最好、平均和最坏时间复杂度均为 `O(n log n)`；空间复杂度为 `O(1)`。该算法
不稳定，因为取出堆顶时会交换距离较远的元素。

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



## 统一测试

此测试让每种算法处理同一个列表。原地算法对副本排序；归并排序和快速排序接收
原列表，并返回新的有序列表。

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
