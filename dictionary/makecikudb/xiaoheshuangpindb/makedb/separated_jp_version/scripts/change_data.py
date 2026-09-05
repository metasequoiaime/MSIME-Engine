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


def update_weight(key: str, value: str, new_key: str, new_value: str, new_weight: int):
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
    old_row = cursor.fetchone()
    if not old_row:
        print(f"No results found key={key}, value={value}")
        conn.close()
        return

    cursor.execute(
        f"UPDATE {tbl_name} SET key=?, value=?, weight=? WHERE key=? AND value=?",
        (new_key, new_value, new_weight, key, value),
    )
    conn.commit()
    print(
        f"Already updated new key={key}, new value={value}, new_weight = {new_weight}"
    )

    conn.close()


if __name__ == "__main__":
    key, value = "yc", "繇"
    new_key, new_value, new_weight = key, value, 1
    update_weight(key, value, new_key, new_value, new_weight)
