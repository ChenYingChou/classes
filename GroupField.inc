<?php
// vim: set ts=4 sw=4:

/** 群組欄位物件: 一個文字欄位包含數組 [key:value]
 * 第一組儲存時不含 "key:", 各組[key:value]之間以 Tab(0x09) 隔開
 *
 * 使用方式:
 *	$gField = new GroupField('姓名,分機,手機,email');
 *	$gField->field = "Tony Chen\t分機:3293\temail:tc@mail.google";
 *	$a = $gField->getArray();
 *	print_r($a);	// output: Array([姓名]=>Tony Chen, [分機]=>3293, [email]=>tc.mail.google
 *	$a['分機'] = '1234';
 *	$a['id'] = 'A11223';
 *	$a['email'] = '';
 *	$gField->setArray($a);
 *	echo $gField->field; // output: Tony Chen\t分機:1234\teid:A11223
 *
 */
class GroupField {
	private
		$_keyList,				// array(key1,key2,...)
		$_valList;				// array(val1,val2,...)

	public
		$allowAddKey;

	function __construct($keyList, $allowAddKey=true) {
		$this->_keyList = strToArray($keyList);
		$this->allowAddKey = $allowAddKey;
	}

	function __get($name) {
		switch($name) {
			case 'field':  // 要回存資料庫欄位的值
				$rs = $this->_valList[0];
				if (substr($rs,0,1) == '{') $rs = ':'.$rs;
				$n = count($this->_keyList);
				for($i=1; $i < $n; $i++) {
					$v = $this->_valList[$i];
					if ($v != '') $rs .= "\t{$this->_keyList[$i]}:$v";
				}
				return $rs;
			default:
				_ERROR("GroupField.__get> Invalid member: [$name].");
		}
	}

	function __set($name, $val) {
		switch($name) {
			case 'field':	// 由資料庫欄位的值存入本物件
				$kv = explode("\t",$val);
				$n = count($kv);
				$this->_valList = array_fill(0,count($this->_keyList),null);
				for ($i=0; $i < $n; $i++) {
					$value = $kv[$i];
					$nPos = strpos($value,':');
					if ($nPos === false) {
						$this->_valList[0] = $value;
					} else {
						$key = trim(substr($value,0,$nPos));
						$nth = array_search($key,$this->_keyList);
						if ($nth === false) { 	// 無對應的 key
							if ($i == 0) {		// 第一個無本來就不用 key
								$this->_valList[0] = $value;
							} else {			// 新增key
								$this->_keyList[] = $key;
								$this->_valList[] = trim(substr($value,$nPos+1));
							}
						} else {				// 找到對應的 key
							$this->_valList[$nth] = trim(substr($value,$nPos+1));
						}
					}
				}
				break;
			default:
				_ERROR("GroupField.__set> Invalid member: [$name].");
		}
	}

	function clear() { $this->field = ''; }
	function keys() { return $this->_keyList; }
	function values() { return $this->_valList; }

	/** 取得 array(key1=>val1, ...)
	 * @return mixed				array(key1=>val1, ...)
	 */
	function getArray() {
		return array_combine($this->_keyList,$this->_valList);
	}

	/** 存入 array(key1->val1, ...)
	 * @param mixed $data 			array(key1->val1, ...)
	 * @return bool 				資料是否異動過
	 */
	function setArray($data) {
		$this->_valList = array_fill(0,count($this->_keyList),null);
		foreach ($data as $key => $value) {
			if (($nth=array_search($key,$this->_keyList)) === false) {
				if (!$this->allowAddKey) {
					_ERROR("GroupField.setArray> no such key: $key");
				}
				if ($value == '') continue;
				$nth = count($this->_valList);
				$this->_keyList[] = $key;
				$this->_valList[] = '';
			}

			$value = str_replace("\t",' ',trim($value));
			$this->_valList[$nth] = $value;
		}
	}

}

?>
