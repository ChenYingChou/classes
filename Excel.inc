<?php
/*
//starting excel
$excel = new COM("excel.application") or die("Unable to instanciate excel");
print "Loaded excel, version {$excel->Version}\n";

//bring it to front
#$excel->Visible = 1;//NOT

//dont want alerts ... run silent
$excel->DisplayAlerts = 0;

//create a new workbook
$wkb = $excel->Workbooks->Add();

//select the default sheet
$sheet=$wkb->Worksheets(1);

//make it the active sheet
$sheet->activate;

//fill it with some bogus data
for($row=1;$row<=7;$row++){
for ($col=1;$col<=5;$col++){

$sheet->activate;
$cell=$sheet->Cells($row,$col);
$cell->Activate;
$cell->value = 'pool4tool 4eva ' . $row . ' ' . $col . ' ak';
}//end of colcount for loop
}

///////////
// Select Rows 2 to 5
$r = $sheet->Range("2:5")->Rows;

// group them baby, yeah
$r->Cells->Group;

// save the new file
$strPath = 'tfile.xls';
if (file_exists($strPath)) {unlink($strPath);}
$wkb->SaveAs($strPath);

//close the book
$wkb->Close(false);
$excel->Workbooks->Close();

//free up the RAM
unset($sheet);

//closing excel
$excel->Quit();

//free the object
$excel = null;
 */

class Excel {
	protected
		$excel,
		$wBook,
		$wSheet,
		$rowNo;

	public
		$keyFieldNumber,	// 每筆資料最前面多少欄位不得全部為空的, 否則該筆視為無資料
		$allowEmptyLines,	// 最多允許多少筆連續空的的資料, 超過則視為檔案結束
		$fields,
		$fieldCount,
		$rowCount;

	public static
		$_keyFieldNumber = 3,
		$_allowEmptyLines = 2;

	/* Constructor: codepage = CP_ACP, CP_UTF7, CP_UTF8 */
	function __construct($fullname=null, $worksheet=1, $codepage=CP_UTF8) {
		$this->excel = new COM("Excel.Application",null,$codepage);
		if (!$this->excel)
			throw new Exception('Can\'t connect Excel COM.');
		if (!empty($fullname))
			$this->open($fullname,$worksheet);
		$this->keyFieldNumber = self::$_keyFieldNumber;
		$this->allowEmptyLines = self::$_allowEmptyLines;
	}

	function __destruct() {
		$this->close();
		$this->excel->Quit();
		$this->excel = null;
	}

	function open($fullname, $worksheet=1) {
		$this->close();
		$this->wBook = $this->excel->Workbooks->Open($fullname);
		if (!$this->wBook)
			throw new Exception("Can\'t open workbook {$fullname}.");
		if (!empty($worksheet))
			$this->worksheet($worksheet);
	}

	function create() {
		$this->close();
		$this->excel->DisplayAlerts = false;
		$this->excel->Application->Visible = false;
		$this->wBook = $this->excel->Workbooks->Add();
		$this->wSheet = $this->wBook->Worksheets(1);
		$this->wSheet->Activate;
	}

	function saveAs($fullname) {
		if (file_exists($fullname)) unlink($fullname);
		$this->wBook->SaveAs($fullname);
	}

	function close() {
		if ($this->wBook) {
			$this->wBook->Close();
			$this->wSheet = null;
			$this->wBook = null;
		}
	}

	/**
	 * @param mixed $worksheet
	 */
	function worksheet($worksheet) {
		$sheet = $this->wSheet = $this->wBook->Worksheets($worksheet);
		if (!$sheet)
			throw new Exception("Can\'t open worksheet {$worksheet}.");

		// 以第一列當成標題, 計算工作簿的最大欄數
		$range = $sheet->UsedRange;
		$this->rowCount = $range->Rows->Count;
		$this->fieldCount = $range->Columns->Count;
		$this->fields = array();
		for ($n = 1; $n <= $this->fieldCount; $n++){
			$value = trim($sheet->Cells(1,$n)->Value);
			if (empty($value)) {
				// 要連續三個空的欄位才視為欄位結束
				$v1 = trim(empty($sheet->Cells(1,$n+1)->Value));
				$v2 = trim(empty($sheet->Cells(1,$n+2)->Value));
				if (empty($v1) && empty($v2)) {
					$this->fieldCount = $n - 1;
					break;
				}
				$value = "Field{$n}";
			}
			$this->fields[] = $value;
		}
		$this->rowNo = 1;
	}

	/** 讀取工作表的第幾列, 第一筆為標題, 資料從第二筆起
	 * @return mixed
	 */
	function cells($row, $col) {
		return $this->wSheet->Cells($row,$col)->Value;
	}

	function storeCell($row, $col, $value) {
		$this->wSheet->Cells($row,$col)->Value = $value;
		if ($row > $this->rowNo) $this->rowNo = $row;
	}

	/** 讀取工作表的第幾列, 第一筆為標題, 資料從第二筆起
	 * @return mixed array
	 */
	function getRow($rowNo) {
		$sheet = $this->wSheet;
		$row = array();
		$isEmpty = true;
		for ($n = 1; $n <= $this->fieldCount; $n++) {
			$value = $sheet->Cells($rowNo,$n)->Value;
			if (is_string($value)) $value = trim($value);
			$row[] = $value;
			if (!empty($value)) {
				$isEmpty = false;
			} else {
				if ($isEmpty && $n >= $this->keyFieldNumber)
					break;
			}
		}
		return $isEmpty ? null : $row;
	}

	/**
	 * @return mixed array
	 */
	function next() {
		$emptyLines = 0;
		while ($this->rowNo < $this->rowCount) {
			$row = $this->getRow(++$this->rowNo);
			if ($row) return $row;
			if (++$emptyLines > $this->allowEmptyLines) {
				$this->rowNo = $this->rowCount;
				break;
			}
		}
		return null;
	}

}

?>
