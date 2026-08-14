"""OnDemand DDS Monitor v2.0 - OnDemand IDL message decoders"""
import logging
from typing import Optional

from .cdr import CDRReader

logger = logging.getLogger(__name__)


def decode_pub_table_define(buf: bytes, offset: int, is_be: bool) -> Optional[dict]:
    """Decode PubTableDefine message.
    Returns: {"name", "nodeName", "description", "var_count", "vars": [{"id", "name", "size", ...}]}
    """
    try:
        r = CDRReader(buf, offset, is_be)
        table_name = r.read_string()
        node_name = r.read_string()
        description = r.read_string()
        count = r.read_u32()
        vars_list = []
        for _ in range(count):
            var_id = r.read_u64()
            disc = r.read_i32()  # VarRequest union discriminator
            if disc == 0:  # VAR_DEFINE
                var_name = r.read_string()
                var_size = r.read_i32()
                model_name = r.read_string()
                model_version = r.read_string()
                var_desc = r.read_string()
                var_node = r.read_string()
                is_readonly = r.read_bool()
                r.align(4)
                publish_mask = r.read_u32()
                vars_list.append({
                    "id": var_id, "name": var_name, "size": var_size,
                    "nodeName": var_node, "model": model_name,
                })
            elif disc == 1:  # VAR_UNDEFINE
                var_name = r.read_string()
                var_node = r.read_string()
                vars_list.append({"id": var_id, "name": var_name, "removed": True})
            else:
                break
        return {
            "name": table_name, "nodeName": node_name,
            "description": description, "var_count": len(vars_list), "vars": vars_list,
        }
    except Exception as e:
        logger.debug("decode_pub_table_define error: %s", e)
        return None


def decode_sub_table_register(buf: bytes, offset: int, is_be: bool) -> Optional[dict]:
    """Decode SubTableRegister message."""
    try:
        r = CDRReader(buf, offset, is_be)
        msg_type = r.read_i32()
        count = r.read_u32()
        vars_list = []
        for _ in range(count):
            nv_name = r.read_string()
            nv_value = r.read_string()
            vars_list.append({"name": nv_name, "freq": nv_value})
        table_name = r.read_string()
        node_name = r.read_string()
        user = r.read_string()
        return {
            "msgType": msg_type, "tableName": table_name,
            "nodeName": node_name, "user": user,
            "vars": vars_list, "var_count": len(vars_list),
        }
    except Exception as e:
        logger.debug("decode_sub_table_register error: %s", e)
        return None


def decode_table_data_transfer(buf: bytes, offset: int, is_be: bool) -> Optional[dict]:
    """Decode TableDataTransfer message."""
    try:
        r = CDRReader(buf, offset, is_be)
        mask_len = r.read_u32()
        mask_bytes = r.read_bytes(mask_len)
        r.align(4)
        tv_sec = r.read_i32()
        tv_nsec = r.read_u64()
        var_data_count = r.read_u32()
        blob_type = r.read_i32()
        return {
            "var_count": var_data_count, "blob_type": blob_type,
            "ts_sec": tv_sec, "ts_nsec": tv_nsec, "mask_len": mask_len,
        }
    except Exception as e:
        logger.debug("decode_table_data_transfer error: %s", e)
        return None