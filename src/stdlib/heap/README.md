# heap：原地二叉堆

`heap` 使用普通 List 保存二叉堆，不引入新的对象类型。默认是数值、String 或 List 字典序最小堆，
也可以通过比较函数处理记录或构建最大堆。

## 接口

```tap
heap::heapify(items: List[, less: Function]) -> Nil
heap::push(items: List, value: AnyType[, less: Function]) -> Nil
heap::pop(items: List[, less: Function]) -> AnyType
heap::peek(items: List) -> AnyType
```

`heapify` 原地建堆，时间 O(n)。`push` 插入元素，均摊时间 O(log n)；
`pop` 移除并返回堆顶，时间 O(log n)；`peek` 读取堆顶，时间 O(1)。
`pop` 先检查替换元素是否已满足堆序，否则先下沉较小子节点，再向上确定替换元素的位置，
减少通常接近叶子的替换元素所需的比较次数；全相等优先级可以直接停止。
该策略适用于默认顺序和任意合法的比较函数，不依赖记录结构或某个算法。
空列表就是空堆，长度和空堆判断使用现有的 `len`。

省略比较函数时，元素可以是 Int、非 NaN Float、String，或由这些值递归组成的 List。
混合数值比较保留大整数精度，允许正负无穷。String 按完整字节内容比较，包含内嵌 NUL。
List 从首项开始，首个不同元素决定顺序；共同前缀相同时短者在前。
数值、String、List 这三类之间不定义顺序；不支持的载荷、NaN、循环或超过 512 层的嵌套报错。
`heapify` 检查全部输入元素，`push` 检查新元素，递归验证为 O(v)，v 为被检查的值总数；
后续堆调整所需比较次数仍为 O(n)/O(log n)，单次比较成本取决于公共前缀长度。
不支持默认比较的载荷请通过比较函数定义优先级。
空堆调用 `pop` 或 `peek` 报运行时错误。

比较函数 `less(a, b)` 必须返回 Bool，表示 a 应排在 b 前面，且满足严格弱序。
同一个堆的所有修改操作必须使用一致的比较规则；比较函数不得修改当前堆或
元素的排序依据。相同优先级不保证出队顺序。比较函数出错时不保证恢复原来的
排列，继续使用前应修正数据并重新建堆。

## 用法

```tapas
let heap_numbers = [5, 2, 4]
heap::heapify(heap_numbers)
heap::push(heap_numbers, 1)
print(heap::pop(heap_numbers))
print(heap::peek(heap_numbers))

let heap_records = [[2, 'later'], [1, 'b'], [1, 'a']]
heap::heapify(heap_records)
print(heap::pop(heap_records)[1])

let heap_less = (a, b)
{
    return a[0] < b[0]
}
let heap_queue = []
heap::push(heap_queue, 8 : 'later', heap_less)
heap::push(heap_queue, 3 : 'earlier', heap_less)
print(heap::pop(heap_queue, heap_less)[1])
```

输出为 `1`、`2`、`a`、`earlier`。最大堆使用 `(a, b) { return a > b }`。

列表别名会看到原地修改。`peek` 和 `pop` 返回原元素，复合对象不做深复制。
建堆后不要直接追加、删除、交换元素或修改优先级；需要这样修改时，应在下一次
堆操作前重新调用 `heapify`。这组接口不进行 O(n) 的堆有效性检查。
