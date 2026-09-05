"""
3. This is third step.

Create index for key and jp.

Each table can hold tens of thousands of rows, indexes are necessary for query performance.
"""

import sqlite3
from pathlib import Path

repo_root = Path(__file__).resolve().parents[4]
import sys
sys.path.insert(0, str(repo_root))
from dictionary_format import pinyin_table, quanpin_tables
db_path = repo_root / "out" / "msime.db"
conn = sqlite3.connect(db_path)
cursor = conn.cursor()

create_index_key_sql = """
create index {} on {}(key);
"""
create_index_jp_sql = """
create index {} on {}(jp);
"""

for cur_tbl in quanpin_tables():
    suffix = cur_tbl.removeprefix("tbl_")
    cursor.execute(create_index_key_sql.format("idx_key_" + suffix, cur_tbl))
    cursor.execute(create_index_jp_sql.format("idx_jp_" + suffix, cur_tbl))

conn.commit()
conn.close()
