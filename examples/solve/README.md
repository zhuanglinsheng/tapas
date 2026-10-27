# solve 示例

- `feasibility.tap` 展示普通可满足性查询。

- `table.tap` 用原生关系表关联商品、价格和库存，通过 Rule 参数传入已知表格，
  再搜索满足预算且有货的完整商品行。

- `generate_violations.tap` 先用 `rules::violate` 反转一条具名业务条件，
  再由 `solve::sample` 生成一份目标场景数据并用 `solve::hold` 检查。
