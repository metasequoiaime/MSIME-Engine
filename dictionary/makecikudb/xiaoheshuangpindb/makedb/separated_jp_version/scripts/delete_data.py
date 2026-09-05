import os.path
import sqlite3
import sys

local_app_data_dir = os.environ.get("LOCALAPPDATA")
db_path = os.path.join(
    local_app_data_dir, "metasequoiaime", "cutted_flyciku_with_jp.db"
)


def choose_tbl(sp_str: str) -> str:
    word_len = len(sp_str) // 2
    if word_len <= 0:
        return "Error"
    base_tbl = "tbl_{}_{}"
    return base_tbl.format(word_len if word_len < 8 else "others", sp_str[0])


def delete_entry(key: str, value: str):
    tbl_name = choose_tbl(key)
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()

    cursor.execute(
        "SELECT name FROM sqlite_master WHERE type='table' AND name=?", (tbl_name,)
    )
    if cursor.fetchone() is None:
        print(f"Table {tbl_name} Not Exists")
        conn.close()
        return

    cursor.execute(f"SELECT * FROM {tbl_name} WHERE key=? AND value=?", (key, value))
    result = cursor.fetchone()
    if not result:
        print(f"No results found key={key}, value={value}")
        conn.close()
        return

    cursor.execute(f"DELETE FROM {tbl_name} WHERE key=? AND value=?", (key, value))
    conn.commit()
    print(f"Already deleted key={key}, value={value}")

    conn.close()


if __name__ == "__main__":
    key = ""
    value = ""
    delete_entry(key, value)
