<?php
// vim: set ts=4 sw=4:

require_once 'common.inc';

abstract class DBTable {
	public
		$removeEmptyArray = true,	// 移除陣列中空的元素
		$autoReviser = false,		// 是否要記錄修改者(reviser, lastUpdated)
		$strictComparer = false;	// 在設定欄位值是否要比較資料型別(是否使用 ===)

	private
		$_table,
		$_fields, 		// 欄位名稱, 各欄之間以逗號隔開(field1,field2,...)
		$_rows, 		// array(array,...): 讀取的資料集(null 或 多筆記錄)
		$_oldRow, 		// 目前編輯中的舊有 _rows[_recPtr]
		$_recPtr, 		// 目前指到那一筆記錄: 0~count(_rows)-1
		$_keyFlds,		// 鍵值欄位名稱(已轉換成小寫): array(field1,field2,...)
		$_keyVals,		// Select/Update/Delete 的鍵值: array(keyVal1,keyVal2,...)
		$_whereKey,		// Where key1=? and key2=? ...
		$_where;		// 額外的 Where 表示式, 不含 "Where" 關鍵字

	private
		$_arrayFlds,	// 那些欄位(名稱已轉成小寫)要自動轉換為陣列: '{...}' -> array()
		$_groupFlds,	// 那些欄位要自動做群組展開或合併?
		$_gfObjs, 		// 群組欄位對應的群組物件array(groupFieldName=>GroupField,...)
		$_fieldInfo,	// 欄位資訊對照表: array(field1=>array(title,type,displayWidth),...)
		$_genParam,		// array(field=>genParamXX,...)
		$_reviser;		// 姓名 (id)

	/**
	 * @param string $table 		資料表名稱
	 * @param mixed $keyFlds		鍵值欄位名稱: array() 或 'key1,key2,...'
	 * @param string $fields='*'	欄位名稱: 'field1,field2,...'
	 * @param mixed $arrayFlds=null 那些欄位為陣列型態, 要自動展開: array() 或 'field1,...'
	 * @param mixed $groupFlds=null 那些欄位為群組型態, 要自動展開: array() 或 'field1,...'
	 * @param GroupField $gfObjs=null	群組欄位對應的群組欄位物件: array(groupField=>GroupField,...)
	 * @param string $where=''		額外過濾條件
	 */
	function __construct($table, $keyFlds, $fields='*', $arrayFlds=null,
 			 			 $groupFlds=null, $gfObjs=null, $where='') {
		$this->_table = $table;
		$this->_keyFlds = strToArrayLowerCase($keyFlds);
		$this->_fields = $fields;
		$this->_recPtr = -1;

		$ff = $fields != '*' ? mb_strtolower(str_replace(' ','',$fields)).',' : '';
		$addFields = '';
		foreach ($this->_keyFlds as $key) {
			if (empty($this->_whereKey)) {
				$this->_whereKey = "WHERE {$key}=?";
			} else {
				$this->_whereKey .= " AND {$key}=?";
			}
			if ($ff !='' && strpos($ff,$key.',') === false) {	// key 不在  fields 中
				$addFields .= ','.$key;
			}
		}
		if (empty($this->_whereKey)) $this->_whereKey = 'WHERE TRUE';
		if (!empty($where)) {
			$this->_whereKey .= " AND {$where}";
			$this->_where = $where;
		}
		if ($addFields != '') $this->_fields .= $addFields;

		$this->setArrayFields($arrayFlds);
		$this->_setGroupFields($groupFlds,$gfObjs);
		$this->_fieldInfo = $this->getFieldInfo();
	}

	/** 根據群組欄位物件, 展開指定的變數(reference variable)
	 * @param GroupField $gfObjs	群組欄位對應的群組欄位物件: array(groupField=>GroupField,...)
	 * @param mixed & $value		參考變數: 將之展開為陣列(允許陣列recursive展開)
	 * @param bool $full			即使沒有值, 也要展開全部欄位
	 */
	protected function expandGroupField($gfObj, &$value, $full=false) {
		if (is_array($value)) {
			if ($full && empty($value)) {
				$val = '';
				$this->expandGroupField($gfObj,$val,$full);
				$value = array($val);
			} else {
				foreach ($value as &$val) {	// allow recursive expanding
					$this->expandGroupField($gfObj,$val,$full);
				}
			}
		} else if($full || !empty($value)) {
			$gfObj->field = $value;
			$value = $gfObj->getArray();
		}
	}

	/** 根據群組欄位物件, 將指定的陣列合併為陣列字串: '{...}'
	 * @param GroupField $gfObjs	群組欄位對應的群組欄位物件: array(groupField=>GroupField,...)
	 * @param mixed $value			群組值之陣列(允許多層陣列合併)
	 * @return string 				陣列字串: '{...}'
	 */
	protected function collapseGroupField($gfObj, $value) {
		if (!is_array($value)) return $value;
		if (is_array(reset($value))) {	// recursive array
			$rs = array();
			foreach ($value as $val) {
				$rs[] = $this->collapseGroupField($gfObj,$val);
			}
			return makeArrayField($rs,$this->removeEmptyArray);
		}
		$gfObj->setArray($value);
		return $gfObj->field;
	}

	function __wakeup() {
		$this->_fieldInfo = $this->getFieldInfo();
	}

	/** 取得現行記錄的欄位值
	 * @param string $name			資料表中的欄位名稱
	 * @return mixed				對應的欄位值, 若欄名在 arrayFlds[]/groupFlds[]
	 *								則會展開成陣列
	 */
	function __get($name) {
		if ($this->eof()) return null;
		$name = mb_strtolower($name);
		$row = $this->_rows[$this->_recPtr];
		if (!array_key_exists($name,$row)) {
			if ($this->_recPtr == 0 && isset($this->_fieldInfo[$name]))
				return null;
			_ERROR("DBTable({$this->_table}).get> No such field: [$name].");
		}
		$rs = $row[$name];
		if (is_string($rs) && substr($rs,0,1) == '{' && in_array($name,$this->_arrayFlds)) {
			$rs = parseArrayField($rs);
		}
		if (!empty($rs) && in_array($name,$this->_groupFlds)) {
			$gfObj = isset($this->_gfObjs[$name]) ? $this->_gfObjs[$name]
					 : new GroupField('ID');
			$this->expandGroupField($gfObj,$rs);
		}
		return $rs;
	}

	/** 設定現行記錄的欄位值
	 * @param string $name			資料表中的欄位名稱
	 * @param mixed $val			對應的欄位值, 若欄名在 arrayFlds[]/groupFlds[]
	 *								則會將陣列合併成陣列字串: '{...}'
	 */
	function __set($name, $val) {
		$name = mb_strtolower($name);
		if (is_string($val)) $val = trim($val);
		if (empty($this->_keyVals)) {	// Insert mode
			if ($this->eof()) {
				$this->_rows = array(array());
				$this->_recPtr = 0;
			}
			$row =& $this->_rows[0];
		} else {						// Update mode
			$row =& $this->_rows[$this->_recPtr];
			if ($this->eof() || !array_key_exists($name,$row)) {
				if (isset($this->_fieldInfo[$name])) return;	// 為擴充欄位
				_ERROR("DBTable({$this->_table}).set> No such field: [$name].");
			}
		}

		// 先轉換 GroupField, 因為可能有欄位為 array & group, 這時要先解決 group,
		// 例如: '{"key1:value1\tkey2:value2...","key1:value1\tkey2:value2..."}'
		if (in_array($name,$this->_groupFlds)) {
			$gfObj = isset($this->_gfObjs[$name]) ? $this->_gfObjs[$name]
						 :	GroupField('ID');
			$val = $this->collapseGroupField($gfObj,$val);
		}

		// 轉換陣列為字串: '{value1,value2,...}'
		if (is_array($val)) {
			$val = makeArrayField($val,$this->removeEmptyArray);
		}

		$old = isset($row[$name]) ? $row[$name] : null;
		$changed = ($this->strictComparer ? $old !== $val : $old != $val);
		if ($changed) {	// 欄位值已改變
			if (empty($this->_oldRow)) $this->_oldRow = $row;
			$row[$name] = $val;
			if (empty($this->_oldRow)) $this->_oldRow = array();
		}
	}

	/** 將欄位值當成 group 欄位展開, 不可帶入已展開的值, 否則會再展開一次造成錯誤的值
	 * @param string $name			欄位名稱, 此名稱必須在 _groupFlds[] 中
	 * @param string $value 		欲展開的原始值, 若為empty則使用現行資料該欄位的值
	 * @param bool $full			即使沒有值, 也要展開全部欄位
	 * @return mixed array			可能為multi-array
	 */
	protected function getGroupFieldValue($name, $value=null, $full=false) {
		$name = mb_strtolower($name);
		$rs = $value;
		if (in_array($name,$this->_groupFlds)) {
			if (empty($rs)) {
				if ($this->eof()) return null;
				$rs = $this->_rows[$this->_recPtr][$name];
			}
			if (is_string($rs) && substr($rs,0,1) == '{' && substr($rs,-1,1) == '}') {
				$rs = parseArrayField($rs);
			}
			if ($full || !empty($rs)) {
				$gfObj = isset($this->_gfObjs[$name]) ? $this->_gfObjs[$name]
							 : new GroupField('ID');
				$this->expandGroupField($gfObj,$rs,$full);
			}
		}
		return $rs;
	}

	abstract protected function getFieldInfo();
	function getTable() { return $this->_table; }
	function getKeyFields() { return $this->_keyFlds; }
	function getKeyValues() { return $this->_keyVals; }
	function getFields() { return $this->_fields; }
	function isChanged() { return !empty($this->_oldRow); }
	function eof() { return $this->_recPtr < 0; }
	function recordCount() { return count($this->_rows); }
	function recordNo() { return $this->_recPtr; }
	function first() { return $this->gotoRecord(0); }
	function next() { return $this->gotoRecord($this->_recPtr+1); }
	function prev() { return $this->gotoRecord($this->_recPtr-1); }
	function last() { return $this->gotoRecord(count($this->_rows)-1); }

	private function fixFields($list) {
		$aList = strToArrayLowerCase($list);
		if ($this->_fields != '*') {
			$aField = strToArrayLowerCase($this->_fields);
			$changed = false;
			for ($i=count($aList); --$i >= 0;) {
				if (!array_key_exists($aList[$i],$aField)) {
					unset($aList[$i]);
					$changed = true;
				}
			}
			if ($changed) return array_values($aList);
		}
		return $aList;
	}

	function setArrayFields($arrayFlds) {
		$this->_arrayFlds = $this->fixFields($arrayFlds);
	}

	protected function _setGroupFields($groupFlds, $gfObjs) {
		$this->_groupFlds = $this->fixFields($groupFlds);
		$this->_gfObjs = $gfObjs;
	}

	/** 根據欄位型別產生 SQL 命令所需參數值
	 * @param string $field		欄位名稱
	 * @param mixed $val		欲設定的欄位值
	 * @return mixed			根據欄位型別轉換成適合 SQL 命令的參數值
	 */
	protected function genParam($field, $val) {
		if (is_null($val)) return $val;
		if (isset($this->_genParam[$field])) return $this->_genParam[$field]($val);
		return genParamString($val);
	}
	/** 設定單一欄位參數轉換函數
	 * @param string $field		欄位名稱
	 * @param function $func	轉換函數名稱, 該函數具一個參數, 應返回適合SQL的值
	 */
	public function setParam($field, $func) {
		$this->genParamInit();
		$this->_genParam[strtolower($field)] = $func;
	}
	/** 設定一群欄位參數轉換函數
	 * @param array $map		array(field=>function,...)
	 */
	public function setParamMap($map) {
		$this->genParamInit();
		foreach ($map as $field => $func) {
			$this->_genParam[strtolower($field)] = $func;
		}
	}
	private function genParamInit() {
		if (!isset($this->_genParam)) {
			$xFunc = array();
			$types = $this->fieldTypes(null,true);
			foreach ($types as $field => $type) {
				switch($type) {
					case 'C': // Char
					case 'S': // String
					case 'V': // Varchar
					case 'T': // Text
						$xFunc[$field] = 'genParamString';
						break;
					case 'I': // Integer
					case 'M': // Money
					case 'N': // Numeric
					case 'F': // Float
					case 'R': // Real
						$xFunc[$field] = 'genParamNumber';
						break;
					case 'D': // Date: YYYY-MM-DD
					case 'X': // Timestamp: YYYY-MM-DD HH:MM:SS[.uuuuuu][+TT:MM]
						$xFunc[$field] = 'genParamDate';
						break;
					case 'AG': // Array & Group
						$xFunc[$field] = 'genParamAG';
						break;
					case 'B': // Boolean
					case 'L': // Logic
						$xFunc[$field] = 'genParamBoolean';
						break;
				}
			}
			$this->_genParam = $xFunc;
		}
	}

	/**
	 * @param array $dataset		對應 table 的資料集(dataset), 由 DBView 指定之
	 */
	function _setRows($dataset) {
		$this->_rows = $dataset;
		$this->first();
	}

	/** 取得目前這筆資料
	 * @param bool $full=false		即使沒有值, 也要展開群組所有的欄位
	 * @param bool $isEmpty=false 	返回全部欄位為空值
	 * @return array				返回目前資料, 若已無資料則為null
	 */
	protected function getRow0($full=false, $isEmpty=false) {
		if ($isEmpty) {
			$row = $this->fieldTypes(null,true);
			foreach ($row as $key => $val) {
				if ($val == '?') {
					unset($row[$key]);
				} else {
					$row[$key] = null;
				}
			}
		} else {
			if ($this->eof()) return null;
			$row = $this->_rows[$this->_recPtr];
		}
		foreach ($this->_arrayFlds as $key) {
			if (isset($row[$key])) {
				$val = $row[$key];
				if (is_string($val) && substr($val,0,1) == '{') {
					$row[$key] = parseArrayField($val);
				} else if ($full && empty($val)) {
					$row[$key] = array();
				}
			} else {
				if ($full) $row[$key] = array();
			}
		}
		foreach ($this->_groupFlds as $key) {
			if (array_key_exists($key,$row)) {
				if ($full || isset($row[$key])) {
					$gfObj = isset($this->_gfObjs[$key]) ? $this->_gfObjs[$key]
								 : new GroupField('ID');
					$this->expandGroupField($gfObj,$row[$key],$full);
				}
			}
		}
		return $row;
	}

	/** 取得空的資料
	 * @param bool $full=true 		是否要展開群組所有的欄位
	 * @return array				返回空的資料記錄, 各欄位為null
	 */
	function getEmpty($full=true) {
		return $this->getRow0($full,true);
	}

	/** 取得目前這筆資料
	 * @param bool $full=false		即使沒有值, 也要展開群組所有的欄位
	 * @return array				返回目前資料, 若已無資料則為null
	 */
	function getRow($full=false) {
		return $this->getRow0($full,false);
	}

	/** 取得資料並將指標指到下筆
	 * @param bool $full=false		即使沒有值, 也要展開群組所有的欄位
	 * @return array				返回資料, 若已無資料則為null
	 */
	function fetchRow($full=false) {
		$row = $this->getRow($full);
		if ($row !== null) $this->next();
		return $row;
	}

	/** 跳到資料集第幾筆(從零算起)
	 * @param int $recNo			第幾筆資料, 從零算起
	 * @return bool 				是否有資料, false表示無資料 ->eof()
	 */
	function gotoRecord($recNo) {
		if ($recNo < 0 || $recNo >= count($this->_rows)) {
			$this->_recPtr = -1;
		} else {
			$this->_recPtr = $recNo;
			$row = $this->_rows[$recNo];
			$this->_keyVals = array();
			foreach ($this->_keyFlds as $fld) {
				$this->_keyVals[] = $row[$fld];
			}
		}
		$this->_oldRow = null;
		return ($this->_recPtr >= 0);
	}

	/** 讀取指定 Key 的資料
	 * @param mixed $keys 			鍵值: array(key1,key2) 或 'key1,key2'
	 * @param string $where=''		SQL WHERE 運算式, 不包含 'WHERE' 關鍵字
	 * @return int					取得筆數(0或1)
	 */
	function get($keys, $where='') {
		$SQL = "SELECT {$this->_fields} FROM {$this->_table} {$this->_whereKey}";
		if (!empty($where)) $SQL .= " AND ($where)";
		$this->_rows = getQueryV($SQL,strToArray($keys));
		$this->first();
		return count($this->_rows);
	}

	/** 自行定義 SQL '[Where ...] [Group ...] [Order ...]' 句子
	 * @param string $whereEtc		SQL WHER 之後的句子 (必須自帶 'Where ...' 完整句子)
	 * @param array $param=null 	$afterWhere 中所需的參數值(使用到 '?' 的地方)
	 * @return int					找到資料筆數, 之後可用 fetchRow()/next() 等讀取資料
	 */
	function find($whereEtc, $param=null) {
		if (!empty($this->_where)) {
			if (strtoupper(substr($whereEtc,0,6)) == 'WHERE ')
				$whereEtc = 'WHERE ' . $this->_where . ' AND ' . substr($whereEtc,5);
			else
				$whereEtc = "WHERE {$this->_where} {$whereEtc}";
		}
		$SQL = "SELECT {$this->_fields} FROM {$this->_table} {$whereEtc}";
		$this->_rows = getQueryV($SQL,$param);
		$this->first();
		return count($this->_rows);
	}

	function refresh() {
		if ($this->eof()) return 0;
		$row = $this->_rows[$this->_recPtr];
		$keyVals = array();
		foreach ($this->_keyFlds as $fld) {
			$keyVals[] = $row[$fld];
		}
		return $this->get($keyVals);
	}

	function lastInsertId() {
		$key = $this->_keyFlds[0];
		if ($this->_recPtr >= 0) {
			$row =& $this->_rows[$this->_recPtr];
			if (isset($row[$key])) {
				$val = $row[$key];
				if (!empty($val)) return $val;
			}
		}
		$result = null;
		try {
			switch(DB()->phptype) {
			  case 'pgsql':
			  	$result = getQueryI("SELECT CURRVAL('{$this->_table}_{$key}_seq')");
			  	break;
			  case 'mysql':
			  	$result = mysql_insert_id();
			  	break;
			}
		} catch(Exception $e) {
			logError("DBTable({$this->_table}).lastInsertId> {$e->getMessage()}.");
		}
		if (!empty($result) && is_array($row)) $row[$key] = $result;
		return $result;
	}

	/** 將異動資料寫回資料庫
	 * @return int					資料異動筆數, null表示未異動, 0表示異動失敗, 1表示成功
	 */
	function post() {
		$rs = null;
		$oldRow = $this->_oldRow;
		if (!empty($oldRow)) {
			$this->genParamInit();
			$param = array();
			$row = $this->_rows[$this->_recPtr];
			if ($this->autoReviser) {
				if (empty($this->_reviser)) {
					if (function_exists('SESSION') && SESSION()->has('user')) {
						$user = SESSION()->user;
						$this->_reviser = "{$user->name} ({$user->id})";
					} else {
						$this->_reviser = '?';
					}
				}
				$row['reviser'] = $this->_reviser;
				$row['lastupdated'] = date('Y-m-d H:i:s');
			}
			if (empty($this->_keyVals)) { 	// Insert
				$SQL = "INSERT INTO {$this->_table} ";
				$COLs = ''; $VALs = ''; $c = '(';
				foreach ($row as $key => $val) {
					$COLs .= "{$c}{$key}";
					$VALs .= "{$c}?";
					$param[] = $this->genParam($key,$val);
					if ($c == '(') $c = ',';
				}
				// $COLs = '(field1,field2,...'
				// $VALs = '(?,?,...'
				$SQL .= "{$COLs}) VALUES{$VALs})";
			} else {						// Update
				$SQL = "UPDATE {$this->_table} SET";
				$c = ' ';
				foreach ($row as $key => $val) {
					$old = isset($oldRow[$key]) ? $oldRow[$key] : null;
					$changed = ($this->strictComparer ? $old !== $val : $old != $val);
					if ($changed) {
						$SQL .= "{$c}{$key}=?";
						$param[] = $this->genParam($key,$val);
						if ($c == ' ') $c = ', ';
					}
				}
			}
			if (count($param) > 0) {
				if (!empty($this->_keyVals)) {	// Update: +' Where key1=? and key2=? ...'
					$SQL .= ' '.$this->_whereKey;
					foreach ($this->_keyVals as $val) $param[] = $val;
				}
				$rs = execSQL($SQL,$param);
			}
			if ($rs) $this->_oldRow = null;
		}
		return $rs;
	}

	/** 異動資料
	 * @param array $aKeyVal=null 	指定欄位要異動的值: array(欄位=>值,...)
	 * @return int					資料異動筆數, null表示未異動, 0表示異動失敗, 1表示成功
	 */
	function update($aKeyVal=null) {
		if (is_array($aKeyVal)) {
			foreach ($aKeyVal as $key => $val) {
				$this->$key = $val;
			}
		}
		return $this->post();
	}

	/** 新增資料
	 * @param array $aKeyVal=null 	指定欄位及對應的值: array(欄位=>值,...)
	 * @return int					資料新增筆數, null表示未新增, 0表示新增失敗, 1表示成功
	 */
	function insert($aKeyVal=null) {
		$this->_rows = $this->_oldRow = $this->_keyVals = null;
		$this->_recPtr = -1;
		if (is_array($aKeyVal)) return $this->update($aKeyVal);
		return null;
	}

	/** 刪除資料
	 * @param array $keys=null		指定要刪除資料的鍵值: array() 或 '鍵值1,鍵值2'
	 * @return int					資料刪除筆數, 0表示刪除失敗, 1表示成功
	 */
	function delete($keys=null) {
		if ($this->_whereKey == 'WHERE TRUE') {
			_ERROR("DBTable({$this->_table}).Delete> No key fields specified, prohibit delete all records.");
		}
		if (empty($keys)) {
			if ($this->eof()) {
				_ERROR("DBTable({$this->_table}).Delete> no record specified.");
			}
			$aKey = $this->_keyVals;
		} else {
			$aKey = strToArray($keys);
		}

		$SQL = "DELETE FROM {$this->_table} {$this->_whereKey}";
		$rs = execSQL($SQL,$aKey);
		if ($rs && empty($keys)) {	// 刪除成功, 且刪除現有資料
			if($this->_recPtr+1 < $this->recordCount()) {
				array_splice($this->_rows,$this->_recPtr,1);
				$this->gotoRecord($this->_recPtr);
			} else {
				$this->gotoRecord(-1);
			}
		}
		return $rs;
	}

	/** 取得 $fieldInfo 其中第幾個值成一陣列
	 * @param array fieldName
	 * @param int nth 				0,1,2,..., 取第幾個值
	 * @param bool bHash			是否產生 associative array
	 * @return array
	 */
	private function _pickFieldInfo($fieldName, $nth, $bHash) {
		$rs = array();
		if (empty($fieldName) || $fieldName == '*') {
			if ($this->eof()) {
				if ($this->_fields == '*') {
					$fieldName = $this->allFieldNames();
				} else {
					$fieldName = strToArray($this->_fields);
				}
			} else {
				$fieldName = array_keys($this->getRow());
			}
		} else if (is_string($fieldName)) {
			$fieldName = strToArray($fieldName);
		}

		foreach ($fieldName as $key) {
			$key = mb_strtolower($key);
			$val = isset($this->_fieldInfo[$key]) ? $this->_fieldInfo[$key] : null;
			if (empty($val)) {
				switch($nth) {
					case 0: // Title
						$v = $key;
						break;
					case 1: // Type
						$v = 'C';
						break;
					case 2: // Display Width
						$v = 8;
						break;
					default:
						$v = '?';
						break;
				}
			} else {
				$v = $val[$nth];
			}
			if ($bHash) $rs[$key] = $v; else $rs[] = $v;
		}
		return $rs;
	}

	/** 取得全部的欄位名稱(轉成小寫)
	 * @return array
	 */
	public function allFieldNames() {
		$rs = array();
		foreach ($this->_fieldInfo as $k => $v) {
			if ($v[1] != '?') $rs[] = strtolower($k);
		}
		return $rs;
	}

	/** 取得各欄位顯示抬頭
	 * @param array fieldName
	 * @param bool bHash=false		是否產生 associative array
	 * @return array
	 */
	public function fieldTitles($fieldName=null, $bHash=false) {
		return $this->_pickFieldInfo($fieldName,0,$bHash);
	}

	/** 取得各欄位對應的資料型別
	 * @param array fieldName
	 * @param bool bHash=false		是否產生 associative array
	 * @return array
	 */
	public function fieldTypes($fieldName=null, $bHash=false) {
		return $this->_pickFieldInfo($fieldName,1,$bHash);
	}

	/** 取得各欄位對應的顯示寬度
	 * @param array fieldName
	 * @param bool bHash=false		是否產生 associative array
	 * @return array
	 */
	public function fieldDispalyWidths($fieldName=null, $bHash=false) {
		return $this->_pickFieldInfo($fieldName,2,$bHash);
	}

}

?>
