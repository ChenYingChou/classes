<?php
// vim: set ts=4 sw=4:

class CodePage {
	protected
		$_OSCP, $_SysCP;

	public function __construct($OSCodePage, $SysCodePage) {
		$this->_OSCP = $OSCodePage;
		$this->_SysCP = $SysCodePage;
	}

	public function __get($nm) {
		switch($nm) {
		  case 'OSCodePage':
			return $this->_OSCP;
		  case 'SysCodePage':
			return $this->_SysCP;
		}
		return null;
	}

	function __set($nm, $val) {
		_ERROR("CodePage.__set> Can't assign '$val' to $nm");
	}

	public function Sys2OS($str) { return $str; }
	public function OS2Sys($str) { return $str; }

	/** 根據參數, 返回不同的 CodePage 物件
	 * @param string $OSCodePage
	 * @param string $SysCodePage
	 * @return object CodePage
	 */
	public static function Create($OSCodePage, $SysCodePage) {
		if (strcasecmp($OSCodePage,$SysCodePage) == 0)
			return new CodePage($OSCodePage,$SysCodePage);
		return new _OSCodePage($OSCodePage,$SysCodePage);
	}
}

class _OSCodePage extends CodePage {
	public function Sys2OS($str) {
		return mb_convert_encoding($str,$this->_OSCP,$this->_SysCP);
	}

	public function OS2Sys($str) {
		return mb_convert_encoding($str,$this->_SysCP,$this->_OSCP);
	}
}

?>
