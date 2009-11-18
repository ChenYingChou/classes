<?php
// vim: set ts=4 sw=4:

require_once 'common.inc';
if (!defined('DATA_PATH')) define('DATA_PATH','');

abstract class CacheObj {
	private
		$_time, $_expired, $_obj, $_fileName, $_fObj;

	function __construct($fileName, $expired=3600) {	// cache 1 hour
		$this->_fObj = pathinfo($fileName,PATHINFO_FILENAME);
		if (dirname($fileName) <= '.') $fileName = DATA_PATH . $fileName;
		$this->_fileName = $fileName;
		$this->_expired = $expired;
	}

	function clear() { $this->_obj = null; return unlink($this->_fileName); }

	abstract protected function getObj();

	private function saveToFile($obj) {
		$fp = @fopen($this->_fileName,"w");
		if ($fp === false) return false;

		$Result = flock($fp,LOCK_EX+LOCK_NB);
		if ($Result) {
			if (empty($obj)) {
				if (isDebug(3)) $tt = microtime(true);
				$this->_obj = $this->getObj();
				if (isDebug(3)) {
					$tt = microtime(true) - $tt;
					@logDebug(sprintf("CacheObj.obj: Call getObj(%s), elapsed %.3f seconds",
									  $this->_fObj,$tt));
				}
			} else {
				$this->_obj = $obj;
			}
			$this->_time = time()+$this->_expired;		// 到期時間
			$s = serialize(array($this->_time,$this->_obj));
			fwrite($fp,$s);
			if (isDebug(1))
				@logDebug("CacheObj.obj: saveToFile('$this->_fileName') Count="
						  .count($this->_obj).' Size='.strlen($s));
		}
		fclose($fp);
		return $Result;
	}

	function __get($nm) {
		switch($nm) {
			case 'time':	// 到期時間
				return $this->_time;
			case 'expired': // 建立物件時, 到期時間 <- 現在時間+expired
				return $this->_expired;
			case 'obj':
				if (isDebug(9)) $t = microtime(true);
				for (;;) {
					if (empty($this->_obj) && file_exists($this->_fileName)) {
						$s = file_get_contents($this->_fileName);
						if (!empty($s)) {
							list($this->_time,$this->_obj) = unserialize($s);
						} else {
							$this->_obj = null;
						}
					}
					if (!empty($this->_obj) && time() <= $this->_time) break;

					if ($this->SaveToFile(null)) break;
					$this->_obj = null;
					usleep(100000);	// 失敗(無法取得物件並儲存之)則等1/10秒再次檢查
				}
				if (isDebug(9))
					@logDebug("CacheObj.obj: get($this->_fObj)="
							  .sprintf('%.6f secs',microtime(true)-$t));
				return $this->_obj;
			case 'fileName':
				return $this->_fileName;
			default:
				_ERROR("CacheObj.__get> Invalid member: $nm");
		}
	}

	function __set($nm, $val) {
		switch($nm) {
			case 'expired':
				$this->_expired = $val;
				break;
			case 'obj':
				while (!$this->SaveToFile($val)) sleep(1);
				break;
			case 'fileName':
				$this->_fileName = $val;
				break;
			default:
				_ERROR("CacheObj.__set> Can't assign '$val' to $nm");
		}
	}
}

?>
