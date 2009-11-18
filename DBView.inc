<?php
// vim: set ts=4 sw=4:

require_once 'DBM.inc';

class DBView extends DBM {
	private
		$_dbTable,
		$_viewFilter;

	/**
	 * @param string $dbTable 		已建立好的 DBTable 物件
	 * @param string $fldTypes='' 	各欄位屬性, 以逗號隔開:
	 *	  字串類: Char | Varchar | String | Text | Date
	 *	非字串類: Binary | Float | Integer |	Logic | Money | Numeric | Real
	 * @param int $pageSize=20		每頁多少筆
	 * @param string $viewFilter	要過濾的算式
	 */
	function __construct(DBTable $dbTable, $fldTypes='', $pageSize=25, $viewFilter='') {
		parent::DBM(null,SYS_CODEPAGE);
		parent::initDB(DB());
		$this->_dbTable = $dbTable;
		$this->_viewFilter = $viewFilter;

		$table = $dbTable->getTable();
		$keyFlds = $dbTable->getKeyFields();
		$fields = $dbTable->getFields();
		$titles = $dbTable->fieldTitles($fields);
		if (empty($fields) || $fields == '*') $fields = $dbTable->allFieldNames();
		if (empty($fldTypes)) $fldTypes = $dbTable->fieldTypes();

		parent::TableName($table);
		parent::FieldName(arrayToCommaStr($fields));
		parent::FieldTitle(arrayToCommaStr($titles));
		parent::PrimaryKey(arrayToCommaStr($keyFlds));
		parent::FieldType(arrayToCommaStr($fldTypes));
		parent::PageSize($pageSize);
		parent::AdditionalVars("\0DBM\0_dataset");
	}

	function __sleep() {
		$this->_dbTable->_setRows(null);
		return parent::__sleep();
	}

	function __wakeup() {
		parent::initDB(DB());
		parent::__wakeup();
	}

	/**
	 * @return DBTable 物件: 包含已讀取的頁面資料, 可用 DBTable 方法讀取各記錄,
	 *						 返回時指標己指到第一筆
	 */
	function getDBTable() {
		$dbTable = $this->_dbTable;
		$dbTable->_setRows(parent::dataset());
		return $dbTable;
	}

	function GetFilter ( ) {
		$cFilterExp = parent::GetFilter();
		if (empty($cFilterExp)) return $this->_viewFilter;
		if (empty($this->_viewFilter)) return $cFilterExp;
		return "{$this->_viewFilter} AND ({$cFilterExp})";
	}
}

?>
