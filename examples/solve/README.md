# solve 示例

`feasibility.tap` 展示普通可满足性查询。
`generate_violations.tap` 先用 `rules::violate` 反转一条具名业务条件，
再由 `solve::sample` 生成一份目标场景数据并用 `solve::hold` 检查。
