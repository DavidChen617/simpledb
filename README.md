# SimpleDB

> 用 C 語言從零實作的關聯式資料庫引擎，具備 B-Tree 索引、Buffer Pool 與資料持久化。

---

## 專案架構

### 指令執行流程（時間軸）

以 `insert employees 1 Alice 95000` 為例：

```
使用者輸入
    │
    ▼
┌─────────────────────────────────────┐
│  REPL  (repl.c)                     │
│  readline() 讀取輸入                │
│  tokenize → ["insert", "employees", │
│              "1", "Alice", "95000"] │
└──────────────────┬──────────────────┘
                   │
                   ▼
┌─────────────────────────────────────┐
│  Executor  (executor.c)             │
│  execute_insert(table, key=1, ...)  │
│  row_serialize → 把值序列化成 bytes │
└──────────────────┬──────────────────┘
                   │
                   ▼
┌─────────────────────────────────────┐
│  B-Tree  (btree.c)                  │
│  table_find → 找到插入位置          │
│  leaf_node_insert → 寫入 cell       │
│  （節點滿了自動 split）             │
└──────────────────┬──────────────────┘
                   │
                   ▼
┌─────────────────────────────────────┐
│  Pager  (pager.c)                   │
│  pager_get_page → 取得記憶體頁面    │
│  pager_mark_dirty → 標記需寫回      │
└──────────────────┬──────────────────┘
                   │
                   ▼
             test.db（disk）
        （執行 .exit 時才真正寫入）
```

### 模組依賴關係（空間圖）

```
┌──────────┐
│   REPL   │
└────┬─────┘
     │
     ▼
┌──────────┐     ┌─────┐     ┌─────────┐
│ Executor │────▶│ Row │     │ Catalog │
└────┬─────┘     └─────┘     └────┬────┘
     │                            │
     ▼                            │ 寄生在 Page 0
┌──────────┐   ┌────────┐    ┌──────────┐
│  B-Tree  │──▶│ Cursor │    │  Pager   │──▶ disk
└────┬─────┘   └────────┘    └────┬─────┘
     │                            │
     └────────────────────────────┘
              都透過 Pager 存取頁面
```

### 各模組職責

| 模組 | 職責 |
|------|------|
| REPL | 讀取輸入、拆 token、呼叫對應函數 |
| Executor | 執行 INSERT / SELECT / UPDATE / DELETE 邏輯 |
| Row | 將欄位值序列化成 bytes，反序列化回來 |
| Catalog | 記錄所有 table 的 schema，持久化在 Page 0 |
| B-Tree | 資料的組織、搜尋（O log n）、節點 split / merge |
| Cursor | 記住目前在 B-Tree 的哪個位置，支援循序掃描 |
| Pager | 管理記憶體 page buffer，負責與 disk 的讀寫 |

---

## 核心概念

### Buffer Pool（Pager）

Pager 是資料庫與 disk 之間的緩衝層。disk 不是一個 byte 一個 byte 讀，而是一次讀一整頁（4096 bytes），這個單位叫 **Page**，對齊現代作業系統與 SSD 的最小 I/O 單位。

```
第一次存取某頁  →  malloc() 配置記憶體  →  read() 從 disk 載入
之後再存取同頁  →  直接回傳記憶體指標，不碰 disk
執行 .exit 時   →  只把 dirty 頁 write() 回 disk
```

**Dirty Bit**：只有被修改過的頁才需要寫回 disk，減少不必要的 I/O。

```c
// 新頁自動標為 dirty（disk 上還不存在）
if (page_num >= pager->num_pages) {
    pager->is_dirty[page_num] = true;
    pager->num_pages++;
}
```

### B-Tree 索引

B-Tree 是這個資料庫的核心，決定了資料怎麼組織與搜尋。

```
                    [ROOT - Internal Node]
               key=3          key=6
              /               |              \
    [Leaf: 1,2,3] ──▶ [Leaf: 4,5,6] ──▶ [Leaf: 7,8,9]
```

- **資料只存在 leaf node**（B+ Tree）
- **Internal node 的 key = 該 child 的 max key**，用來路由
- **Leaf node 之間用 next_leaf 串成 linked list**，`SELECT *` 掃全表直接沿鏈走
- 搜尋複雜度：**O(log n)**
- 節點滿了 → **split**，空了 → 向兄弟借或 **merge**

### Cursor

Cursor 是 B-Tree 上的位置指標，記錄「目前在哪」：

```c
typedef struct Cursor {
    Table    *table;
    uint32_t  page_num;     // 在哪一頁
    uint32_t  cell_num;     // 那頁的第幾個 cell
    bool      end_of_table;
} Cursor;
```

`SELECT *` 的實作就是：從 `table_start` 拿到第一個 cursor，一直 `cursor_advance` 到 `end_of_table`。

---

## Pointer 與 Malloc 實際應用

### 1. `void *` 萬用指標（泛型設計）

Pager 不在乎頁面裡存的是什麼，一律用 `void *` 代表「一塊記憶體」：

```c
void *pages[MAX_PAGES];  // 每格是 4096 bytes，型別由上層決定
```

上層使用時再 cast 成需要的型別：

```c
// B-Tree 把它當 leaf node 的 cell 用
uint32_t *num_cells = (uint32_t *)((char *)node + LEAF_NODE_NUM_CELLS_OFFSET);

// Catalog 把它當 struct 用
memcpy(catalog, page, sizeof(Catalog));
```

### 2. `malloc` 與手動記憶體管理

每次 cursor 用完都需要手動釋放，否則記憶體洩漏：

```c
Cursor *cursor = table_find(table, key);   // malloc 在內部
// ... 使用 cursor
free(cursor);                               // 手動釋放
```

### 3. 指標運算計算 byte offset

B-Tree 的 node 是一塊連續記憶體，用指標運算存取各個欄位：

```c
// leaf node 的第 N 個 cell 在哪裡
void *leaf_node_cell(const Table *table, void *node, uint32_t cell_num) {
    return (char *)node + LEAF_NODE_HEADER_SIZE + cell_num * table->cell_size;
}

// cell 裡的 key（前 4 bytes）
uint32_t *leaf_node_key(const Table *table, void *node, uint32_t cell_num) {
    return (uint32_t *)leaf_node_cell(table, node, cell_num);
}
```

---

## 支援的指令

### DDL
```
create table <name> (<col> <type>, ...)
drop table <name>
describe <table>
```

### DML
```
insert <table> <id> <val>...
select * from <table>
select <col1>, <col2> from <table> [where <col> <op> <val>]
update <table> <id> <val>...
delete <table> <id>
```

### Meta 指令
```
.list              列出所有 table
.btree <table>     印出 B-Tree 結構
.exit              離開並存檔
```

### WHERE 運算子
- INT 欄位：`=` `>` `<` `>=` `<=`
- TEXT 欄位：`=` `>` `<`

---

## 編譯與執行

**環境需求：** gcc、libreadline

```bash
make
./simpledb mydb.db
```

**快速測試：**
```
create table employees (id INT, name TEXT, salary INT)
insert employees 1 Alice 95000
insert employees 2 Bob 82000
insert employees 3 Carol 110000
select * from employees
select name, salary from employees where salary > 90000
.btree employees
.exit
```
