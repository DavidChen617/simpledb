Pager
- 管理記憶體 page buffer（pages[]）和 disk 的搬運
- 第一次存取某頁 → malloc + read() 從 disk 載入
- 已在記憶體 → 直接回傳指標，不碰 disk
- 新頁（page_num >= num_pages）→ malloc 空記憶體，自動標 dirty
- 關閉時只把 dirty 頁 write() 回 disk
- lseek(fd, page_num * PAGE_SIZE, SEEK_SET) 定位到正確頁

  ---
Catalog
- 寄生在 Page 0，存所有 table 的 metadata
- 用 memcpy 把 Catalog struct 直接序列化到 Page 0
- 開啟 db 時 catalog_load 讀回來，關閉時 catalog_flush 寫回去
- 讓重開程式後還知道有哪些 table、欄位定義、root page 在哪

  ---
B-Tree
- 資料只存在 leaf node，internal node 只存路由 key
- internal node 的 key = 該 child 的 max key
- leaf node 之間用 next_leaf 串成 linked list（支援全表掃描）
- 滿了 → split，空了 → borrow 或 merge
- 搜尋 O(log n)，全表掃描直接走 leaf linked list

  ---
Cursor
- 只是一個位置描述（page_num + cell_num）
- table_find → B-Tree 搜尋，回傳 cursor
- cursor_value → 回傳該位置的 row data 指標
- cursor_advance → 移到下一格，跨 leaf 走 next_leaf
- SELECT * = 從 table_start 一直 advance 到 end_of_table

  ---
Table
- 把 Pager、TableMeta、root_page_num 集中在一起
- table_open 時預先計算 cell_size、max_cells、min_cells
- B-Tree 所有函數只需傳一個 Table *，不需分別傳各個資訊
- 是設計圖（schema）+ 倉庫（pager）的橋接層