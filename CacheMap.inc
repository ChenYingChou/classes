<?php
// vim: set ts=4 sw=4:

require_once 'CacheObj.inc';

class CacheMap extends CacheObj {
	private
		$_sql, $_map, $_nth, $_toKeys;

	public
		$mode = DB_FETCHMODE_ORDERED;	// DB_FETCHMODE_ASSOC

	function __construct($table, $fields, $expired=28800, $others='', $order=1) { // cache 8 hours
		if (is_int($others)) {
			$order = $others;
			$others = '';
		}
		if ($order < 1) $order = 1;
		if (stripos($others,'Order By ') === false) {
			$others = trim("{$others} Order by {$order}");
		}
		$this->_sql = trim("Select {$fields} from {$table} {$others}");
		$this->_nth = $order - 2;	// nth of array(_map[key])
		$table = preg_replace('/[\s\/\'",=+]+/','_',$table);
		parent::__construct("{$table}.map.obj",$expired);
	}

	private function _initMap() {
		if (empty($this->_map)) {
			$this->_map = $this->obj;	// obj 第一次使用會自動載入
			$nth = $this->_nth;
			if ($nth >= 0) {
				$toKeys = array();
				foreach ($this->_map as $key => $val) {
					$v = $val[$nth];
					if (!empty($v)) $toKeys[$v] = $key;
				}
				$this->_toKeys = $toKeys;
			}
		}
	}

	protected function getObj() {
		return getQueryMap($this->_sql,null,$this->mode);
	}

	/** 取得 CacheMap 鍵對應的值 (key => value)
	 * @param string $key 			鍵值
	 * @param mixed $index=0		若有多個值, 可用$index表示那個值
	 *								當 $mode = DB_FETCHMODE_ASSOC 時可為字串索引
	 *								若為空白則表示將多個值以陣列返回, 若僅單一值則只為該值
	 * @param mixed &$retKey=null	若不為null則表示要返回讀取到的key值, 用在 $order > 1 時
	 * @return mixed				如為多個值, 且 $index='' 則返回值的陣列, 否則為單一值
	 *								若原key對應值為陣列, 且指定索引值為空的, 則返回第一個非空值
	 */
	function get($key, $index=0, &$retKey=null) {
		if (is_null($key)) return null;
		$this->_initMap();
		if ($this->_nth >= 0) {	// 優先尋找指定值(若該值存在的話)
			if (isset($this->_toKeys[$key])) {
				$key = $this->_toKeys[$key];
			}
		}
		if (isset($retKey)) $retKey = $key;
		$rs = isset($this->_map[$key]) ? $this->_map[$key] : '';
		if (is_array($rs) && $index !== '') {
			$val = $rs[$index];
			if (!empty($val)) return $val;
			foreach($rs as $val) {
				if (!empty($val)) return $val;
			}
			return null;
		}
		return $rs;
	}

	function getKeys() {
		$this->_initMap();
		return array_keys($this->_map);
	}
}

?>
