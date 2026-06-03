# SimpleDB v2 重構計畫

> 用途：未來帶著使用者一步一步重寫 v2 時的參考文件。
> 每個 Phase 都是獨立可編譯的里程碑。

---

## v1 問題清單

### Bug（真實缺陷）

| # | 檔案 | 行號 | 問題 | 後果 |
|---|------|------|------|------|
| B1 | `pager.c` | 36 | `page_num > MAX_PAGES` 應為 `>=` | page_num==100 時存取 `pages[100]`，陣列越界（UB） |
| B2 | `cursor.c` | 24 | `next_page == 0` 判斷無意義 | Page 0 是 Catalog，leaf 的 next_leaf 正常不會是 0，但語意不明確；應只用 `INVALID_PAGE_NUM` |

### 設計問題

| # | 檔案 | 問題 | 教學重點 |
|---|------|------|----------|
| D1 | `pager.c` | 沒有 dirty bit，`db_close` 把全部 cached page 寫磁碟 | Buffer Pool 的 dirty page 概念 |
| D2 | `pager.h` | `pager_flush(pager, page_num, size)` 的 `size` 每次都是 `PAGE_SIZE` | 假彈性；移除多餘參數 |
| D3 | `table.c` | `db_close` 直接 loop 所有 pages 做 flush | 職責歸位：flush 決策應在 Pager |
| D4 | `btree.h`↔`cursor.h` | 循環 include：btree.h→cursor.h→table.h，cursor.h 又宣告 table_find（定義在 btree.c） | 循環依賴是架構警訊；用 forward declaration 打破 |
| D5 | `cursor.h` L19 + `btree.h` L76 | `table_find` 在兩個 header 都宣告 | 重複宣告，Single source of truth |
| D6 | `cursor.h` L16 | `table_end` 宣告但從未實作 | Dead declaration，link error 陷阱 |
| D7 | `table.c` | `serialize_row` / `deserialize_row` 從未被呼叫 | Dead code 造成閱讀誤導 |
| D8 | `btree.c` | `internal_node_cell_ptr`(static) 與 `internal_node_cell`(public) 計算完全一樣 | 重複邏輯 |
| D9 | `btree.h` L35 | `#define INTERNAL_NODE_MAX_CELLS 3` 是刻意縮小的測試值，完全無說明 | Magic number 隱藏意圖 |
| D10 | `catalog.h` | `sizeof(Catalog)≈3876`，只比 4096 少一點，沒有 compile-time 保護 | static_assert 的使用時機 |
| D11 | `main.c` | insert/update/delete 各有一段相同的 table 解析邏輯（複製貼上三份） | DRY 原則 |

---

## 教學順序（Phase by Phase）

每個 Phase 目標：**可以 make + 執行，功能和 v1 相同**，只是內部更乾淨。

---

### Phase 1：修 pager — Dirty Bit + API 清理

**動到的檔案：** `pager.h`, `pager.c`, `table.c`（db_close）

**改動清單：**

1. `pager.h` — 加入 `bool is_dirty[MAX_PAGES]`
2. `pager.h` — 移除 `pager_flush` 的 `size` 參數
3. `pager.h` — 新增 `pager_mark_dirty(pager, page_num)`
4. `pager.c:get_page` — 新頁（page_num >= num_pages）自動 mark dirty；從磁碟載入的頁 is_dirty=false
5. `pager.c:pager_flush` — 移除 size 參數，永遠寫 PAGE_SIZE
6. `pager.c:pager_close` — 只 flush dirty pages（原本是在 db_close 做這件事）
7. `pager.c:get_page` — 修正 B1：`> MAX_PAGES` → `>= MAX_PAGES`
8. `table.c:db_close` — 移除手動 flush loop，改由 pager_close 內部處理

**教學問題（寫完問學生）：**
- Q: 為什麼新頁要「自動」mark dirty，而從磁碟載入的頁不用？
- Q: 如果程式 crash（不走 .exit），dirty pages 沒有落盤，資料怎麼辦？（引出 WAL 概念）
- Q: `pager_close` 現在只 flush dirty pages，原本 v1 全刷的做法有什麼問題？

**具體 code diff（pager.h）：**
```c
// v1
typedef struct {
    int fd;
    uint32_t file_length;
    uint32_t num_pages;
    void *pages[MAX_PAGES];
} Pager;
void pager_flush(Pager *pager, uint32_t page_num, uint32_t size);

// v2
typedef struct {
    int      fd;
    uint32_t num_pages;
    void    *pages[MAX_PAGES];
    bool     is_dirty[MAX_PAGES];   // 新增
} Pager;
void pager_mark_dirty(Pager *pager, uint32_t page_num);  // 新增
void pager_flush(Pager *pager, uint32_t page_num);       // 移除 size
```

**具體 code diff（pager.c get_page 關鍵段）：**
```c
// v1
if (page_num > MAX_PAGES) { ... }  // BUG: 應是 >=

// v2
if (page_num >= MAX_PAGES) { ... }  // 修正

// v1：從磁碟載入後沒有設 dirty
// v2：區分新舊頁
if (page_num < pager->num_pages) {
    lseek(...); read(...);
    // is_dirty[page_num] 保持 false（clean，從磁碟來的）
} else {
    pager->is_dirty[page_num] = true;   // 新頁，還沒落盤
    pager->num_pages = page_num + 1;
}
```

**具體 code diff（pager.c pager_close）：**
```c
// v1
void pager_close(Pager *pager) {
    for (int i = 0; i < MAX_PAGES; i++) {
        if (pager->pages[i]) { free(pager->pages[i]); }
    }
    close(pager->fd); free(pager);
}

// v2 — 在 close 時順便 flush dirty pages
void pager_close(Pager *pager) {
    for (int i = 0; i < MAX_PAGES; i++) {
        if (pager->pages[i]) {
            if (pager->is_dirty[i])
                pager_flush(pager, (uint32_t)i);   // 只刷 dirty
            free(pager->pages[i]);
        }
    }
    close(pager->fd); free(pager);
}
```

**具體 code diff（table.c db_close）：**
```c
// v1
void db_close(Database *db) {
    catalog_flush(db->pager, &db->catalog);
    for (uint32_t i = 0; i < db->pager->num_pages; i++) {   // 這段刪掉
        if (db->pager->pages[i])
            pager_flush(db->pager, i, PAGE_SIZE);
    }
    pager_close(db->pager);
    free(db);
}

// v2 — 簡潔；flush 責任移給 pager_close
void db_close(Database *db) {
    catalog_flush(db->pager, &db->catalog);  // mark page 0 dirty
    pager_close(db->pager);                  // 只刷 dirty，free 所有 pages
    free(db);
}
```

---

### Phase 2：打破循環 Include + 清理重複宣告

**動到的檔案：** `btree.h`, `cursor.h`, `cursor.c`

**改動清單：**

1. `btree.h` — 移除 `#include "cursor.h"`，改用 `struct Cursor;` forward declaration
2. `cursor.h` — 移除 `table_find` 宣告（它屬於 btree 模組）
3. `cursor.h` — 移除 `table_end` 宣告（從未實作）
4. `cursor.h` — `typedef struct Cursor { ... }` 加上 struct tag（配合 forward declaration）
5. `cursor.c` — 確認 `#include "btree.h"` 在此，取得 `table_find` 等函數

**教學問題：**
- Q: 為什麼 btree.h 要用 forward declaration 而不是直接 include cursor.h？
- Q: `struct Cursor;` 和 `typedef struct Cursor Cursor;` 有什麼差？什麼時候只能用 pointer（`Cursor*`）而不能用值（`Cursor`）？
- Q: `table_find` 屬於 btree 模組還是 cursor 模組？怎麼判斷？

**v1 vs v2 dependency graph：**
```
v1（有循環）:
  btree.h → cursor.h → table.h
  cursor.h → (table_find 宣告在這，但定義在 btree.c)

v2（無循環）:
  pager.h ← schema.h ← catalog.h ← table.h ← btree.h
                                             ↖ cursor.h
  cursor.c → btree.h（取得 table_find 等函數）
```

**具體 code diff（btree.h 開頭）：**
```c
// v1
#include "cursor.h"   // 造成循環

// v2
#include "table.h"
struct Cursor;         // forward declaration，不需要知道 Cursor 的內容
```

**具體 code diff（cursor.h）：**
```c
// v1
Cursor *table_start(Table *table);
Cursor *table_end(Table *table);    // 刪除（從未實作）
void   *cursor_value(Cursor *cursor);
void    cursor_advance(Cursor *cursor);
Cursor *table_find(Table *table, uint32_t key);  // 刪除（移到 btree.h）

// v2（加上 struct tag）
typedef struct Cursor {
    Table    *table;
    uint32_t  page_num;
    uint32_t  cell_num;
    bool      end_of_table;
} Cursor;

Cursor *table_start(Table *table);
void   *cursor_value(Cursor *cursor);
void    cursor_advance(Cursor *cursor);
```

---

### Phase 3：清理 btree.c 的重複與魔術數字

**動到的檔案：** `btree.h`, `btree.c`

**改動清單：**

1. `btree.c` — 刪除 `internal_node_cell_ptr`，統一使用 `internal_node_cell`
2. `btree.h` — 新增 `INTERNAL_NODE_CAPACITY` 常數（真實容量）
3. `btree.h` — 在 `INTERNAL_NODE_MAX_CELLS 3` 旁加上明確說明
4. `btree.c` — 在所有修改現有頁的地方加入 `pager_mark_dirty` 呼叫

**教學問題：**
- Q: 算算看 `INTERNAL_NODE_CAPACITY` 實際是多少？（`(4096-14)/8 = 510`）
- Q: 改成 510 之後，需要插入多少筆資料才能觸發 internal node split？
- Q: 為什麼 `left_split_count = (max+1) - right_split_count` 而不是直接 max/2？

**具體 code diff（btree.h）：**
```c
// v1
#define INTERNAL_NODE_MAX_CELLS 3   // 意圖不明

// v2
// 一個 internal node 頁面的物理容量上限
#define INTERNAL_NODE_CAPACITY \
    ((PAGE_SIZE - INTERNAL_NODE_HEADER_SIZE) / INTERNAL_NODE_CELL_SIZE)

// 強制提早分裂，方便觀察 B-Tree 行為。
// 改成 INTERNAL_NODE_CAPACITY 即可使用完整容量。
#define INTERNAL_NODE_MAX_CELLS 3
```

**需要加 pager_mark_dirty 的函數清單：**
```
leaf_node_insert       → mark dirty: cursor->page_num
leaf_node_delete       → mark dirty: cursor->page_num
leaf_node_handle_underflow:
  borrow from right    → mark dirty: page_num, right_page, parent_page
  borrow from left     → mark dirty: page_num, left_page, parent_page
  merge right          → mark dirty: page_num, parent_page
  merge into left      → mark dirty: left_page, parent_page
leaf_node_split_and_insert → mark dirty: cursor->page_num (old_node)
  ↳ new_node 是新頁，pager_get_page 時自動 dirty
create_new_root        → mark dirty: table->root_page_num
  ↳ left_child 是新頁，自動 dirty
internal_node_insert   → mark dirty: parent_page_num
catalog_flush          → mark dirty: CATALOG_PAGE（page 0）
```

---

### Phase 4：移除死碼 + Catalog static_assert

**動到的檔案：** `table.h`, `table.c`, `catalog.h`

**改動清單：**

1. `table.h` / `table.c` — 刪除 `serialize_row`, `deserialize_row`（死碼）
2. `catalog.h` — 加入 `static_assert(sizeof(Catalog) <= PAGE_SIZE, ...)`

**教學問題：**
- Q: `sizeof(Catalog)` 實際是多少？（算法：4 + 8×(32+4+4+4+10×(32+4+4+4)) = 4 + 8×484 = 3876）
- Q: 3876 < 4096，那為什麼還需要 static_assert？（如果有人把 MAX_TABLES 改成 9 呢？）
- Q: static_assert 和 runtime assert 的差別是什麼？何時用哪個？

**具體 code diff（catalog.h）：**
```c
#include <assert.h>

// Catalog 必須塞進一個 Page，否則 memcpy 會靜默覆蓋相鄰 page
static_assert(sizeof(Catalog) <= PAGE_SIZE,
    "Catalog exceeds one page — reduce MAX_TABLES or MAX_COLUMNS");
```

---

### Phase 5：整理 main.c — 統一 table 解析邏輯

**動到的檔案：** `main.c`

**改動清單：**

1. 新增 `resolve_dml_table()` helper，把 insert/update/delete 共用的 table 解析邏輯抽出來
2. 清理三處重複的解析段落

**v1 重複的段落（insert/update/delete 各一份）：**
```c
char **vals = tokens + 1; int nvals = ntok - 1;
Table *t = active;
if (nvals > 0 && catalog_find(&db->catalog, vals[0]) >= 0) {
    t = table_open(db, vals[0]); opened = 1; vals++; nvals--;
}
```

**v2 抽成 helper：**
```c
// vals: tokens+1（指令動詞之後）
// 若 vals[0] 是已知 table name，開它並位移 vals
// opened: 若為 1，呼叫者需要 table_close
static Table *resolve_dml_table(Database *db, Table *active,
                                 char ***vals, int *nvals, int *opened) {
    *opened = 0;
    if (*nvals > 0 && catalog_find(&db->catalog, (*vals)[0]) >= 0) {
        Table *t = table_open(db, (*vals)[0]);
        if (!t) return NULL;
        (*vals)++; (*nvals)--;
        *opened = 1;
        return t;
    }
    return active;
}
```

---

## 完整的 v2 Header Include 圖

```
pager.h          (無本地依賴)
schema.h         (無本地依賴)
catalog.h   →    pager.h, schema.h
table.h     →    pager.h, schema.h, catalog.h
btree.h     →    table.h  +  struct Cursor; (forward)
cursor.h    →    table.h
cursor.c    →    cursor.h, btree.h
btree.c     →    btree.h, cursor.h
table.c     →    table.h, btree.h
catalog.c   →    catalog.h
main.c      →    table.h, cursor.h, btree.h
```

---

## 每個 Phase 完成後的驗收方式

```bash
make
./simpledb test.db
> create table users (id INT, name TEXT(50), age INT)
> insert users 1 alice 30
> insert users 2 bob 25
> insert users 3 charlie 30
> select * from users where age = 30
(1, alice, 30)
(3, charlie, 30)
> update users 2 bob 26
> delete users 3
> select * from users
(1, alice, 30)
(2, bob, 26)
> .btree
> .exit
# 重開後資料還在
./simpledb test.db
> select * from users
(1, alice, 30)
(2, bob, 26)
```

---

## 每個元件最終應回答的問題

下次帶學生寫完每個 Phase 後，可以問這些問題確認理解深度。

| 元件 | 核心問題 |
|------|---------|
| Pager | dirty page 什麼時候設？什麼時候清？crash 後 dirty page 丟失怎麼辦？ |
| Pager | `num_pages` 和「磁碟上實際有幾頁」什麼時候不一致？ |
| B-Tree | `create_new_root` 為何把 root 內容搬到 left_child，而不是直接讓 root 當 left_child？ |
| B-Tree | `find_child_index_in_parent` 回傳 `num_keys` 代表什麼？ |
| B-Tree | leaf node 的 `next_leaf` linked list 是什麼時候建立的？誰在維護它？ |
| Catalog | Catalog 為何放在 Page 0 而不是最後一頁？ |
| Cursor | `table_start` 為何呼叫 `table_find(table, 0)` 而不是直接取 root page 的第一個 cell？ |
