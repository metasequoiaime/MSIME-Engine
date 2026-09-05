"""Load the public table format API from the reviewed Engine gitlink."""
import importlib.util
from pathlib import Path

path = Path(__file__).resolve().parent / 'vendor/MetasequoiaImeEngine/contracts/dictionary/format.py'
if not path.is_file():
    raise RuntimeError('Initialize vendor/MetasequoiaImeEngine before building dictionaries')
spec = importlib.util.spec_from_file_location('msime_dictionary_format_contract', path)
contract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(contract)
FORMAT = contract.FORMAT
pinyin_table = contract.pinyin_table
quanpin_tables = contract.quanpin_tables

product_spec = importlib.util.spec_from_file_location('msime_dictionary_product_contract', path.with_name('product.py'))
product_contract = importlib.util.module_from_spec(product_spec)
product_spec.loader.exec_module(product_contract)
verify_product = product_contract.verify_product
