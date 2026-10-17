# fs：文件系统操作

Linux/macOS 的同步文件系统接口。path 只处理路径文本，本包实际访问文件系统。
路径要求非空 String 且不含 NUL，不要求 UTF-8，不执行 shell 展开。

| 接口 | 返回及语义 |
| --- | --- |
| `stat(path: String)` | Dictionary；跟随符号链接。 |
| `lstat(path: String)` | Dictionary；查询末段链接本身，包括悬空链接。 |
| `exists(path: String)` | Bool；跟随链接，仅不存在或路径中间项不是目录时为 false，其他查询错误仍报错。 |
| `list_dir(path: String)` | 按字节排序的名称 List，不包含 `.`、`..`，不是完整路径。 |
| `mkdir(path: String, [parents: Bool])` | Nil；默认单级创建，已存在报错；parents=true 时逐级创建并接受已有目录。 |
| `copy(source: String, target: String)` | Nil；复制普通文件数据，拒绝源链接与特殊文件，拒绝覆盖已有目标。 |
| `rename(source: String, target: String)` | Nil；POSIX rename 语义，可替换已有目标；跨文件系统报错。 |
| `remove(path: String)` | Nil；删除文件、末段符号链接或空目录，不递归。 |
| `temp_dir([parent: String])` | String；创建唯一目录，初始权限 0700。 |
| `temp_file([parent: String])` | String；创建唯一空文件并关闭句柄，初始权限 0600。 |

临时项默认放在非空 TMPDIR，否则 `/tmp`；返回的路径由调用方负责清理。
创建目录使用 0777、复制目标使用 0666，再受调用进程 umask 限制。
copy 仅复制内容，不复制权限、时间、所有者或扩展属性，也不承诺原子可见性或源数据快照。
失败时清理本次创建的目标；mkdir 的 parents 模式失败不会回滚已创建的父目录。
rename 的原子性由文件系统提供，不提供跨设备复制回退。

stat/lstat 字段：`kind` 为 file/directory/symlink/other，`size` 为字节数，
`permissions` 为 POSIX 权限与特殊位，`modified` 为秒精度 Time。
目录大小和其他文件类型的元数据遵循平台；size 超出 Int 范围时报错。
这些操作具有通常的文件系统竞态，不构成访问控制边界。

## 可恢复接口

每个接口都有同名 `try_` 版本，参数位置相同，并接受 AnyType 以记录输入类型错误。
结果字段为 `ok`、`value`、`code`、`error`、`errno`，与 io 的尝试接口一致：

- stat/lstat 失败 value 是空 Dictionary；list_dir 是空 List；temp_dir/temp_file 是空 String。
- exists 成功 value 是实际 Bool（包括 false）；失败也是 false，但 ok=false。
- 修改操作成功 value=true，失败为 false。
- 成功 code/error 为空串、errno=0；失败 code 包括 invalid_type、invalid_path、out_of_range、not_found、
  permission_denied、is_directory、not_directory、already_exists、not_empty、no_space、symlink_loop、cross_device、io_error。
- error 为平台诊断文本，errno 为平台错误号；输入校验错误 errno=0。资源耗尽仍可能产生运行时错误。

```tapas
let fs_example_root = fs::temp_dir()
let fs_example_file = path::join(fs_example_root, 'data')
io::write_text(fs_example_file, 'hello')
print(fs::stat(fs_example_file)::kind)
print(len(fs::list_dir(fs_example_root)))
fs::remove(fs_example_file)
fs::remove(fs_example_root)
```

<pre class='Tapas-Return'>
file
1
</pre>
