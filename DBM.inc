<?php
// vim: set ts=2 sw=2:

require_once "DB.php";

//
// 檢查是否為空的陣列: 若非陣列即視為空的陣列
//
function isEmptyList ( $aList )
{
  if ( !is_array($aList) ) return true;
  foreach ( $aList as $value ) {
    if ( isset($value) && $value != '' ) return false;
  }
  return true;
}

class DBM {
/****************************************************************************
 * Private: Variables (//後符號表示可經由property存取, '=':可讀寫, '+':唯讀)
 ****************************************************************************/
  private
    $_inLoading_        = false,        // 不可變更此變數 -- 僅在__weakup()中設定
    $_AdditionalVars    = array(),      //=在serialize()時要多加那些變數(array)
    $_ErrorHalt         = true,         //=發生錯誤時是否結束程式
    $_DebugMode         = false;        //=發生錯誤時是否要印出物件內容

  private
    $_db,                               //+資料庫, DB::connect(...)
    $_dataset           = array(),      //+資料集合, db->getAll(cSQL,DB_FETCHMODE_ASSOC)
    $_ReversedRecord    = false,        // 相反順序, LastPage,PrevPage
    $_CurrPtr           = -1,           //+0~count(_dataset)-1
    $_Encoding          = '',           //=Client端編碼
    $_FixCaseLower      = true,         //=欄位名稱轉成小寫
    $_hasOffset         = true;         //=SQL支援 OFFSET xx

  private
    $_TableName         = '',           //=Table name(可用JOIN)
    $_FilterExp         = '',           //=Filter Expression
    $_PageSize          = 15,           //=每頁資料數, 零表示不限筆數(全部)
    $_isAscending       = true;         //=是否按昇冪

  private
    $_RecordCount       = 0,            //+總資料數
    $_PageCount         = 0,            //+總頁數
    $_CurrentPage       = 0,            //+目前頁次:1~PageCount
    $_SQLCurrPage       = '';           //+目前頁面的SQL命令

  private
    $_Timeout           = 600,          //=超過600秒就重新計算總筆數
    $_TimerRecount      = 0;            // 下次要重新計算總筆數的系統時間

  private
    $_FieldNameStr      = '',           //='a.id as id, b.id as bid, ...'
    $_FieldNameStr_     = '',           // 將_FieldNameStr中無效的空白去掉
    $_FieldNameList     = array(),      //+[a.id] [b.id] ...
    $_AliasNameList     = array();      //+ [id]   [bid]  ...

  private
    $_FieldTypeStr      = '',           //='X,X,...', X=Char|String|Text|Date|Memo|Num
    $_FieldTypeList     = array();      //+[C] [S] [T] [D] [N]

  private
    $_FieldTitleStr     = '',           //=DBGrid抬頭:'title1,title2,...'
    $_FieldTitleList    = array();      //+[title1] [title2] ...

  private
    $_SortAllFields     = true,         //=每次指定SortName時會先清除
    $_SortNameStr       = '',           //='Alias1=keylist;Alias2=keylist;...'
    $_SortNameList      = array();      //+Association Array: [Alias1]=[key,key] [Alias2]=[key,key] ...

  private
    $_PrimaryKeyStr     = '',           //=主鍵值:'key1,key2,...'
    $_KeyNameStr        = '',           //=排序值:'a.id,b.id,...'
    $_KeyNameList       = array();      //+[a.id] [b.id] ...

  private
    $_KeyFirstList      = array(),      //+第一筆Keys的內容
    $_KeyLastList       = array(),      //+最末筆Keys的內容
    $_KeyCurrList       = array();      //+目  前Keys的內容


function __sleep ( )
{
  $ptr = $this->_CurrPtr;
  if ( isEmptyList($this->_KeyCurrList ) ) $this->KeyCurrList();
  if ( isEmptyList($this->_KeyFirstList) ) $this->KeyFirstList();
  if ( isEmptyList($this->_KeyLastList ) ) $this->KeyLastList();
  $this->_CurrPtr = $ptr;

//  $X = array(   // 不備份: db, _dataset, ..., _inLoading_
//    '_ReversedRecord','_CurrPtr','_hasOffset',//'_db','_dataset',
//    '_FixCaseLower','_Encoding',
//    '_TableName','_FilterExp','_PageSize','_isAscending',
//    '_RecordCount','_PageCount','_CurrentPage','_SQLCurrPage',
//    '_Timeout','_TimerRecount',
//    '_FieldNameStr',//'_FieldNameStr_','_FieldNameList','_AliasNameList',
//    '_FieldTypeStr',//'_FieldTypeList',
//    '_FieldTitleStr',//'_FieldTitleList',
//    '_SortAllFields','_SortNameStr',//'_SortNameList',
//    '_PrimaryKeyStr','_KeyNameStr',//'_KeyNameList',
//    '_KeyFirstList','_KeyLastList','_KeyCurrList',
//    '_AdditionalVars','_ErrorHalt','_DebugMode'//,'_inLoading_'
//  );
  $X = array_diff(array_keys((array)$this),
        array("\0DBM\0_db"            ,"\0DBM\0_dataset"      ,"\0DBM\0_FieldNameStr_",
              "\0DBM\0_FieldNameList" ,"\0DBM\0_AliasNameList","\0DBM\0_FieldTypeList",
              "\0DBM\0_FieldTitleList","\0DBM\0_SortNameList" ,"\0DBM\0_KeyNameList"  ,
              "\0DBM\0_inLoading_"    )
       );

  return empty($this->_AdditionalVars)
       ? $X
       : array_unique(array_merge($X,$this->_AdditionalVars));
}

function __wakeup ( )
{
  $this->_inLoading_ = true;

  $this->FieldName($this->_FieldNameStr);
  $this->FieldType($this->_FieldTypeStr);
  $this->FieldTitle($this->_FieldTitleStr);
  $this->KeyName($this->_KeyNameStr);
  $this->PrimaryKey($this->_PrimaryKeyStr);
  $this->SortName($this->_SortNameStr);
  if ( !isset($this->_dataset) ) $this->_dataset = array();

  $this->_inLoading_ = false;
}

/****************************************************************************
 * Public: (I) Constructor & Functions
 ****************************************************************************/
//
// Constructor
//
function DBM ( $cDBConnStr=null, $cEncoding=null )
{
  if ( is_string($cEncoding) ) $this->_Encoding = $cEncoding;
  if ( is_string($cDBConnStr) ) $this->connectDB($cDBConnStr);
}

function connectDB ( $cDBConnStr )     // 開啟新的資料庫連結
{
  $this->_db = DB::connect($cDBConnStr);
  if ( DB::isError($this->_db) ) {
    // 去掉username/password, 例如:
    // 'mysql://username:password@host/db' -> 'mysql://***:***@host/db'
    $cConnStr = preg_replace('|://.+@|','://***:***@',$cDBConnStr);
    $this->_ERROR_("connectDB('$cConnStr') -> ".$this->_db->getMessage());
    return null;
  }
  if ( $this->_db->phptype == 'pgsql' ) {
    $rc = $this->_db->connection;       // resource connection of pgsql
    if ( $this->_Encoding != '' ) {
      if ( strcasecmp(pg_client_encoding($rc),$this->_Encoding) != 0 )
        pg_set_client_encoding($rc,$this->_Encoding);
    }
  } else if ( $this->_db->phptype == 'mysql' ) {
    if ( $this->_Encoding != '' )
      $this->_db->query("set names '$this->_Encoding'");
  } else if ( $this->_db->phptype == 'odbc' ) {
    $this->_hasOffset = false;
    //$this->_FixCaseLower = false;
    $this->_db->setOption('portability',DB_PORTABILITY_LOWERCASE);
  }
  return $this->_db;
}

function initDB ( $dbObj )             // 直接使用外界已建立連結的資料庫
{
  if ( !is_subclass_of($dbObj,'DB_common') ) {
    $this->_ERROR_("initDB('$dbObj') -> 不是DB::connect()產生的物件");
    return null;
  }

  $this->_db = $dbObj;
  return true;
}

//
// 確認TableName/FieldName皆已指定好
//
function isValid ( )
{
  return is_subclass_of($this->_db,'DB_common')
         and ($this->_TableName <> '')
         and (count($this->_FieldNameList) > 0);
}

// property db: TClientDataset read _db;
function db ( )
{
  return $this->_db;
}

// property dataset0: OleVariant read _dataset;
function dataset0 ( )      // 允許外界直接[接觸]到內部的_dataset (可能反向排序)
{
  return $this->_dataset;
}

// 差不多接近讀取_dataset的意思, 但在降冪時必須反向重排一次
// property dataset: OleVariant read get_dataset;
function dataset ( )          // 按小到大排序好, _dataset不受外界影響
{
  $cnt = count($this->_dataset);
  if ( $cnt == 0 || !$this->_ReversedRecord ) return $this->_dataset;

  $rows = array();
  for ( $i = $cnt ; --$i >= 0 ; ) {
    $rows[] = $this->_dataset[$i];
  }
  return $rows;
}

// property AdditionalVars: string read _AdditionalVars write _AdditionalVars
function AdditionalVars ( $cAdditionalVars=null )
{
  if ( isset($cAdditionalVars) ) {
    // 由於 serialize 物件時 private 變數會在前面加上 "\0類別名稱\0", 而 protected 變數
    // 則前面加上 "\0*\0", 因此不可直接用 trim() 會將最前面的 NULL 字元去掉.
    // 而 perl regular expression 的 "\s" 空白字元僅包含 " \t\n\r\f", 不影響 NULL
    $this->_AdditionalVars = is_array($cAdditionalVars)
                  ? $cAdditionalVars
                  : preg_split('/\s*,\s*/',trim($cAdditionalVars," \t\r\n\f"));
    return true;
  } else {
    return $this->_AdditionalVars;
  }
}

// property ErrorHalt: boolean read _ErrorHalt write _ErrorHalt
function ErrorHalt ( $bErrorHalt=null )
{
  if ( isset($bErrorHalt) ) {
    $this->_ErrorHalt = $bErrorHalt;
    return true;
  } else {
    return $this->_ErrorHalt;
  }
}

// property DebugMode: boolean read _DebugMode write _DebugMode
function DebugMode ( $bDebugMode=null )
{
  if ( isset($bDebugMode) ) {
    $this->_DebugMode = $bDebugMode;
    return true;
  } else {
    return $this->_DebugMode;
  }
}

// property hasOffset: boolean read _hasOffset write _hasOffset
function hasOffset ( $bhasOffset=null )
{
  if ( isset($bhasOffset) ) {
    $this->_hasOffset = $bhasOffset;
    return true;
  } else {
    return $this->_hasOffset;
  }
}

// property Encoding: string read _Encoding write _Encoding
function Encoding ( $sEncoding=null )
{
  if ( isset($sEncoding) ) {
    $this->_Encoding = $sEncoding;
    return true;
  } else {
    return $this->_Encoding;
  }
}

// property FixCaseLower: boolean read _FixCaseLower write _FixCaseLower
function FixCaseLower ( $bFixCaseLower=null )
{
  if ( isset($bFixCaseLower) ) {
    $this->_FixCaseLower = $bFixCaseLower;
    return true;
  } else {
    return $this->_FixCaseLower;
  }
}

function _FixCase_ ( $string )
{
  if ( $this->_Encoding == '' ) {
    return $this->_FixCaseLower
         ? strtolower($string)
         : strtoupper($string);
  } else {
    return $this->_FixCaseLower
         ? mb_strtolower($string,$this->_Encoding)
         : mb_strtoupper($string,$this->_Encoding);
  }
}

function more ( )             // 目前資料指標是否還有效?
{
  return $this->_CurrPtr >= 0 && $this->_CurrPtr < count($this->_dataset);
}

function FirstRecord ( )
{
  return $this->GotoRecord(0);
}

function LastRecord ( )
{
  $this->_CurrPtr = count($this->_dataset) - 1;
  return $this->_CurrPtr;
}

function GotoRecord ( $RecNo )
{
  if ( $RecNo < 0 || $RecNo >= count($this->_dataset) ) {
    $this->_CurrPtr = -1;
  } else {
    $this->_CurrPtr = $RecNo;
  }
  return $this->_CurrPtr;
}

function SkipRecord ( $cnt=1 )
{
  return $this->GotoRecord($this->_CurrPtr+$cnt);
}

function NextRecord ( )
{
  return $this->GotoRecord($this->_CurrPtr+1);
}

function PrevRecord ( )
{
  return $this->GotoRecord($this->_CurrPtr-1);
}

function GetRecord ( )       // 取得目前資料(單筆)內容
{
  if ( !$this->more() ) return null;
  if ( $this->_ReversedRecord ) {
    return $this->_dataset[count($this->_dataset)-1-$this->_CurrPtr];
  } else {
    return $this->_dataset[$this->_CurrPtr];
  }
}

// property CurrPtr: integer read _CurrPtr;
function CurrPtr ( )          // 目前資料的編號(指標): 0 ~ count(_dataset)-1
{
  return $this->_CurrPtr;
}

//
// function GetQueryI(const cSQL: string): integer;
//
// Get first column from SQL result
//
function GetQueryI ( $cSQL )
{
  $val = $this->_db->getOne($cSQL);
  if ( DB::isError($val) ) {
    $this->_ERROR_("SQL('$cSQL') -> ".$val->getMessage(),$this->_DebugMode);
    return null;
  }
//!!echo "<br/>GetQueryI('$cSQL')=$val<br/>";
  return (int)$val;
}

//
// function GetQueryV(const cSQL: string): OleVariant;
//
// Get dataset from SQL result
//
function GetQueryV ( $cSQL )
{
  $rows = $this->_db->getAll($cSQL,null,DB_FETCHMODE_ASSOC);
  if ( DB::isError($rows) ) {
    $this->_ERROR_("SQL('$cSQL') -> ".$rows->getMessage(),$this->_DebugMode);
    return null;
  }
//!!echo "<br/>GetQueryV('$cSQL')<br/>"; var_dump($rows);
  return $rows;
}

/****************************************************************************
 * Private: Functions
 ****************************************************************************/

function _ERROR_ ( $cErrMsg, $bDumpObj=false )
{
//  echo "\n>>> DBM::$cErrMsg\n";
//  if ( $bDumpObj ) print_r($this);
//  if ( $this->_ErrorHalt ) trigger_error($cErrMsg,E_USER_ERROR);
  if ( $this->_ErrorHalt ) _ERROR($cErrMsg,$this);
//  _ERROR($cErrMsg,$bDumpObj?$this:null);
}

//
// 新的一頁, 將舊有的 Key{First|Last|Curr}List 清除
//
function _new_page_ ( $nPage, $cSQL=null )
{
  if ( !empty($cSQL) ) {
    $this->_SQLCurrPage = $cSQL;
    $this->_dataset = $this->GetQueryV($cSQL);
    $this->FirstRecord();
  }
  $this->_CurrentPage = $nPage;
  $this->_KeyFirstList = array();
  $this->_KeyLastList = array();
  $this->_KeyCurrList = array();
}

//
// 當 [_dataset是空的] 或 [超過時間] 均應更新資料
//
function _need_refresh_ ( )
{
  $Result = false;
  if ( empty($this->_dataset) ) {
    $Result = true;
  } else if ( time() >= $this->_TimerRecount ) {
    $this->RecordCount();
    $Result = true;
  }
  return $Result;
}

//
// procedure fetch_current_key(xList: TStringList);
//
// 記錄現有資料記錄的Key值 --
// 針對資料庫dsDBM.DataSet目前記錄, 將KeyNameList所指欄位的值, 以字串方式
// 儲存在TStringList xList中
//
// 註:KeyNameList[]中的欄位名稱可能帶有'table.', 而DELPHI TField的欄位
//    名稱卻不含'table.', 因此要透過ToKeyAlias()將之轉成FKeyAliasList[]
//
function _fetch_current_key_ ( &$xList )
{
  $Row = $this->GetRecord();
  $cnt = count($this->_KeyNameList);
  $xList = array();
  for ( $i = 0 ; $i < $cnt ; $i++ ) {
    $xList[] = $Row[$this->ToKeyAlias($i)];
  }
  return true;
}

//
// 檢查FieldType[Key(nth)]: 若是字串/日期類則返回true, 否則為false
// FieldType:
//   字串類: Char | Varchar | String | Text | Date | Array | Group | Boolean | Logic
// 非字串類: Float | Integer | Money | Numeric | Real
//
function _is_text_type_ ( $nthKey )
{
  $nth = array_search($this->_KeyNameList[$nthKey],$this->_FieldNameList);
  if ( $nth === false ) return true;
  $C = substr($this->_FieldTypeList[$nth],0,1);
  if ( $C == '' ) return true;
  return strchr('FIMNR',$C) === false;
}

//
// 將cSQL句子加上' LIMIT [nCount] OFFSET [nFrom]
//
function _modify_limit_query_ ( $cSQL, $nFrom, $nCount )
{
  if ( $nFrom <= 0 && $nCount <= 0 ) return $cSQL;
  if ( $nCount <= 0 ) $nCount = 9999999;
  if ( $this->_hasOffset )
    return $this->_db->modifyLimitQuery($cSQL,$nFrom,$nCount);
  if ( $nFrom > 0 )
    $this->_ERROR_('資料庫不支援位移:'.
                   $this->_db->modifyLimitQuery($cSQL,$nFrom,$nCount),
                   $this->_DebugMode);
  return $cSQL.' FETCH FIRST '.$nCount.' ROWS ONLY';
}

//
// function SQL_Select: string;
//
// 輸出: 'SELECT [FieldName] FROM [TableName]'
// 例如: SELECT a.id as id,a.name as name,b.id as bid
//         FROM table a LEFT JOIN table b ON a.id=b.id
//
// 使用來源:
//   FieldName = 'a.id as id,a.name as name,b.id as bid'
//   TableName = 'table a LEFT JOIN table b ON a.id=b.id'
//
function _SQL_Select_ ( )
{
  return 'SELECT '.$this->FieldName().' FROM '.$this->_TableName;
}

//
// function SQL_Where: string; overload;
// function SQL_Where(const OP: string; ValueList: TStringList): string; overload;
//
// 輸出:
//     空字串
//  或 'WHERE ([FilterExp])'
//  或 'WHERE ([FilterExp]) AND (Key1 [OP] Val1 OR Key1=Val1 AND Key2 [OP] Val2 ...)'
//  或 'WHERE (Key1 [OP] Val1 OR Key1=Val1 AND Key2 [OP] Val2 ...)'
//
// 使用來源:
//   KeyNameList[] -> [Key1] [Key2] ...
//   ValueList[]   -> [Val1] [Val2] ...
//
function _SQL_Where_ ( $OP=null, $ValueList=null )
{
  $cFilterExp = $this->GetFilter();
  $Result = ($cFilterExp=='') ? '' : " WHERE ({$cFilterExp})";
  if ( !is_array($ValueList) ) return $Result;

  $n = count($ValueList);
  $cExp = '';

  $cOP = str_replace('=','',$OP);
  if ( $cOP != $OP ) {   // 含有'='
    // Key1=Val1 AND Key2=Val2 ...
    for ( $i = 0 ; $i < $n ; $i++ ) {
      if ( $this->_is_text_type_($i) ) {
        $cCurr = $this->_KeyNameList[$i]."=".$this->_db->quoteSmart($ValueList[$i]);
      } elseif ( $ValueList[$i] == '' ) {  // 不可使用empty()函數, 可能為'0'
        $cCurr = $this->_KeyNameList[$i].' IS NULL';
      } else {
        $cCurr = $this->_KeyNameList[$i].'='.$ValueList[$i];
      }
      $cExp = ($cExp=='') ? $cCurr : $cExp.' AND '.$cCurr;
    }
  }

  if ( $cOP != '' ) {
    if ( !$this->_isAscending ) $cOP = strtr($cOP,'><','<>');
    // (Key1 [OP] Val1) OR (Key1=Val1 AND Key2 [OP] Val2) ...
    // 註: Descending時碰到Key值為NULL欄位時會不正確
    $cLast = '';
    for ( $i = 0 ; $i < $n ; $i++ ) {
      if ( $this->_is_text_type_($i) ) {
        $cVal = $this->_db->quoteSmart($ValueList[$i]);
        $cCurr = $this->_KeyNameList[$i].$cOP.$cVal;
        $cNext = $this->_KeyNameList[$i].'='.$cVal;
      } elseif ( $ValueList[$i] == '' ) {  // 不可使用empty()函數, 可能為'0'
        $cCurr = $this->_KeyNameList[$i].' IS NOT NULL';
        $cNext = $this->_KeyNameList[$i].' IS NULL';
      } else {
        $cCurr = $this->_KeyNameList[$i].$cOP.$ValueList[$i];
        $cNext = $this->_KeyNameList[$i].'='.$ValueList[$i];
      }

      if ( $cExp == '' ) {
        $cExp = $cCurr;
        $cLast = $cNext;
      } elseif ( $cLast == '' ) {
        $cExp .= " OR $cCurr";
        $cLast = $cNext;
      } else {
        $cExp .= " OR $cLast AND $cCurr";
        $cLast .= " AND $cNext";
      }
    }
  }

  if ( $cExp != '' ) {
    if ( $Result == '' ) {
      $Result = " WHERE ($cExp)";
    } else {
      $Result .= " AND ($cExp)";
    }
  }

  return $Result;
}

//
// function SQL_Order(isAscending: boolean=true): string;
//
// 輸出:
//   'ORDER BY a.key1, a.key2, ...'
//   'ORDER BY a.key1 DESC, a.key2 DESC, ...'
//
// 使用來源:
//   KeyName = 'a.key1,a.key2,...'
//
function _SQL_Order_ ( $isAscending=true )
{
  $Result = ' ORDER BY '.$this->KeyName();
  if ( $this->_isAscending != $isAscending ) {
    $Result = str_replace(',',' DESC,',$Result).' DESC';
  }
  return $Result;
}


/****************************************************************************
 * Protected: Properties & Functions
 ****************************************************************************/

function Reset ( )
{
  $this->_dataset = array();
  $this->_ReversedRecord = false;
  $this->_CurrPtr = -1;
  $this->_PageCount = 0;
  $this->_RecordCount = 0;
  $this->_new_page_(0);
  $this->_isAscending = true;
  $this->_FilterExp = '';
}

//
// property TableName: string read FTableName write set_TableName;
//
// 起始設定資料庫 -- 將相關資訊一律清除
//
function TableName ( $cTableName=null )
{
  if ( isset($cTableName) ) {
    if ( $this->_TableName != $cTableName ) {
      $this->Reset();
      $this->_TableName = $cTableName;
      $this->FieldName('');
      $this->FieldTitle('');
      $this->PrimaryKey('');
      $this->KeyName('');
      $this->SortName('*');       // 預設值: 允許任意欄位+PrimaryKey
    }
    return true;
  } else {
    return $this->_TableName;
  }
}

//
// property FieldName: string read FFieldNameStr write set_FieldName;
//
// 設定欄位及別名資訊 --
// 輸入字串: 即SQL SELECT的欄位部份表示法, 如: 'a.id as id, b.id as bid, ...'
//
// 將輸入字串拆開, 分別存入FFieldNameList及FAliasNameList, 如下所示:
//   FFieldNameList[] = [a.id] [b.id] ...
//   FAliasNameList[] = [id]   [bid]  ...
//
  // 無法使用巢狀函數定義(nested function?)
  function _RemoveTableName_ ( $cFld )
  {
    $n = strpos($cFld,'.');
    return $n === false ? $cFld : substr($cFld,$n+1);
  }
function FieldName ( $cFieldName=null )
{
  if ( isset($cFieldName) ) {
    $cFieldName = $this->_FixCase_($cFieldName);
    if ( $this->_FieldNameStr != $cFieldName || $this->_inLoading_ ) {
      // _FieldNameStr  <--> 'a.id as id, b.id as bid, ...'
      // _FieldNameStr_ ---> 'a.id as id,b.id as bid,...'
      // _FieldNameList ---> [a.id] [b.id] ...
      // _AliasNameList ---> [id]   [bid]  ...
      $this->_FieldNameStr = $cFieldName;
      $this->_FieldNameStr_ = '';
      $this->_FieldNameList = array();
      $this->_AliasNameList = array();

      // 不可將空白當成分隔欄位符號
      $aFld = preg_split('/\s*[,;]\s*/',trim($cFieldName),-1,PREG_SPLIT_NO_EMPTY);
      $nCnt = count($aFld);
      for ( $i = 0 ; $i < $nCnt ; $i++ ) {
        $cFld = $aFld[$i];
        if ( empty($cFld) ) {
          $this->_ERROR_("FieldName('$cFieldName') -> 含有空的欄位",$this->_DebugMode);
          return null;
        }

        if ( $i > 0 ) $this->_FieldNameStr_ .= ',';
        $nPos = strpos(strtoupper($cFld),' AS ');
        if ( $nPos === false ) {
          $this->_FieldNameList[] = $cFld;
          $this->_AliasNameList[] = $this->_RemoveTableName_($cFld);
          $this->_FieldNameStr_ .= $cFld;
        } else {
          $a = trim(substr($cFld,0,$nPos));
          $b = trim(substr($cFld,$nPos+4));
          $this->_FieldNameList[] = $a;
          $this->_AliasNameList[] = $b;
          $this->_FieldNameStr_ .= "$a AS $b";
        }
      }
    }
    return true;
  } else {
    return $this->_FieldNameStr_;
  }
}

//
// 將欄位別名轉換成含table的欄位名稱 --
//
// 例如SQL命令: SELECT a.id as id, b.id as bid FROM ...
// 在DELPHI的各式元件中均使用alias名稱(不含'table.', 如: TField.FieldName),
// 但自行產生的SQL命令卻必須要使用包含table名稱(如: WHERE a.id='A123')
//
function AliasToFieldName ( $cAlias )
{
  $cAlias = $this->_FixCase_($cAlias);
  $i = array_search($cAlias,$this->_AliasNameList);
  return ($i === false) ? $cAlias : $this->_FieldNameList[$i];
}

function FieldToAliasName ( $cField )
{
  $cField = $this->_FixCase_($cField);
  $i = array_search($cField,$this->_FieldNameList);
  return ($i === false) ? $cField : $this->_AliasNameList[$i];
}

//
// property FieldType: string read get_FieldTitle write set_FieldTitle;
//
function FieldType ( $cFieldType=null )
{
  if ( isset($cFieldType) ) {
    if ( $this->_FieldTypeStr != $cFieldType || $this->_inLoading_ ) {
      $this->_FieldTypeStr = $cFieldType;
      $this->_FieldTypeList = preg_split('/\s*[,;]\s*/',$cFieldType,-1,PREG_SPLIT_NO_EMPTY);
    }
    return true;
  } else {
    return join(',',$this->_FieldTypeList);
  }
}

//
// property FieldTitle: string read get_FieldTitle write set_FieldTitle;
//
function FieldTitle ( $cFieldTitle=null )
{
  if ( isset($cFieldTitle) ) {
    if ( $this->_FieldTitleStr != $cFieldTitle || $this->_inLoading_ ) {
      $this->_FieldTitleStr = $cFieldTitle;
      $this->_FieldTitleList = preg_split('/\s*[,;]\s*/',trim($cFieldTitle),-1,PREG_SPLIT_NO_EMPTY);
    }
    return true;
  } else {
    return join(',',$this->_FieldTitleList);
  }
}

//
// property FilterExp: string read FFilterExp write FFilterExp;
//
function FilterExp ( $cFilterExp=null )
{
  if ( isset($cFilterExp) ) {
    if ($this->_FilterExp != $cFilterExp) {
      $this->_FilterExp = $cFilterExp;
      $this->_RecordCount = 0;
    }
    return true;
  } else {
    return $this->_FilterExp;
  }
}

//
// public GetFilter: string; virtual;
//
function GetFilter ( )
{
	return $this->FilterExp();
}

//
// property PrimaryKey: string read FPrimaryKeyStr write set_PrimaryKey;
//
function PrimaryKey ( $cPrimaryKey=null )
{
  if ( isset($cPrimaryKey) ) {
    $cPrimaryKey = $this->_FixCase_($cPrimaryKey);
    if ( $this->_PrimaryKeyStr != $cPrimaryKey || $this->_inLoading_ ) {
      $this->_PrimaryKeyStr = $cPrimaryKey;
      if ( $this->KeyName() == '' ) $this->KeyName($cPrimaryKey);
    }
    return true;
  } else {
    return $this->_PrimaryKeyStr;
  }
}

//
// property KeyName: string read get_KeyName write set_KeyName;
//
// 基本上KeyName呼叫get_KeyName, 得到結果應同FKeyNameStr, 也就是當初指定給
// KeyName的字串(重新組合後, 可能會去掉部份空白)
// 如: 'a.id,a.pro_type,a.s_date'
//
function KeyName ( $cKeyName=null )
{
  if ( isset($cKeyName) ) {
    $cKeyName = $this->_FixCase_($cKeyName);
    if ( $this->_KeyNameStr != $cKeyName || $this->_inLoading_ ) {
      $oldKeyNameStr = $this->_KeyNameStr;
      $oldKeyNameList = $this->_KeyNameList;
      $this->_KeyNameStr = $cKeyName;
      $this->_KeyNameList = preg_split('/\s*[,;]\s*/',$cKeyName,-1,PREG_SPLIT_NO_EMPTY);
      for ( $i = count($this->_KeyNameList) ; --$i >= 0 ; ) {
        $cFld = $this->_KeyNameList[$i];
        if ( array_search($cFld,$this->_KeyNameList) == $i ) {  // 並未重覆
          if ( array_search($cFld,$this->_FieldNameList) === false ) {  // 轉換成table.field
            $n = array_search($cFld,$this->_AliasNameList);
            if ( $n === false ) {
              $this->_KeyNameStr = $oldKeyNameStr;
              $this->_KeyNameList = $oldKeyNameList;
              $this->_ERROR_("KeyName('$cKeyName') -> 無此欄位:[$cFld]",$this->_DebugMode);
              return null;
            }
            $this->_KeyNameList[$i] = $this->_FieldNameList[$n];
          }
        } else {    // 去掉重複的key
          array_splice($this->_KeyNameList,$i,1);
        }
      }
      // if ( $this->CurrentPage() > 0 ) ) $this->FirstPage(true);  // 不同於DELPHI, 不需立即更新
      if ( !$this->_inLoading_ ) {
        $this->_KeyFirstList = array();
        $this->_KeyLastList = array();
        $this->_KeyCurrList = array();
      }
    }
    return true;
  } else {
    return join(',',$this->_KeyNameList);
  }
}

//
// property KeyAlias: string read get_KeyAlias;
//
// 同KeyName, 但使用alias名稱, 用於指定給TClientDataSet.IndexFieldNames,
// 當作排序鍵值使用(DELPHI元件只使用別名, table名稱皆會去掉)
// 如: 'id;pro_type;s_date'
//
function KeyAlias ( )
{
  $Result = '';
  $nCnt = count($this->_KeyNameList);
  for ( $i = 0 ;  $i < $nCnt ; $i++ ) {
    if ( $i > 0 ) $Result .= ';';
    $s = $this->_KeyNameList[$i];
    $n = array_search($s,$this->_FieldNameList);
    $Result .= ($n === false) ? $s : $this->_AliasNameList[$n];
  }
  return $Result;
}

//
// 將KeyNameList[nth]轉換成alias名稱(不含table名稱)
// 如: 'a.id' --> 'id'
//
function ToKeyAlias ( $nth )
{
  $Result = $this->_KeyNameList[$nth];
  $i = array_search($Result,$this->_FieldNameList);
  return ($i === false) ? $Result : $this->_AliasNameList[$i];
}

//
// property SortAllFields: boolean read FSortAllFields write FSortAllFields;
//
function SortAllFields ( $bSortAllFields=null )
{
  if ( isset($bSortAllFields) ) {
    $this->_SortAllFields = $bSortAllFields;
    return true;
  } else {
    return $this->_SortAllFields;
  }
}

//
// property SortName: string read get_SortName write set_SortName;
//
// 基本上SortName呼叫get_SortName, 得到結果應同FSortNameStr, 也就是當初指定給
// SortName的字串(重新組合後, 可能會去掉部份空白)
// 如: 'Field1=key1,key2;Field2=key3,key4'
//
// 特例:
//   '*'    : 預設值, 允許使用任意欄位+PrimaryKey來排序
//   空字串 : 不允許變更Sort Key, 使用KeyName(預設值同PrimaryKey)
//
function SortName ( $cSortName=null )
{
  if ( isset($cSortName) ) {
    $cSortName = $this->_FixCase_($cSortName);
    if ( $this->_SortNameStr != $cSortName || $this->_inLoading_ ) {
      // _SortNameStr  <--> 'Alias1=key1,key2; Alias2=key3,key4; ...'
      // _SortNameList ---> [Alias1]=[key1,key2] [Alias2]=[key3,key4] ...
      if ( !$this->_inLoading_ ) $this->_SortAllFields = false;
      $this->_SortNameStr = $cSortName;
      $this->_SortNameList = array();

      $aKey = preg_split('/;+/',str_replace(' ','',$cSortName),-1,PREG_SPLIT_NO_EMPTY);
      $nCnt = count($aKey);
      for ( $i = 0 ; $i < $nCnt ; $i++ ) {
        $cFld = $aKey[$i];
        if ( empty($cFld) ) {
          $this->_ERROR_("SortName('$cSortName') -> 含有空的欄位",$this->_DebugMode);
          return null;
        }
        if ( $cFld == '*' ) {
          $this->_SortAllFields = true;
        } else {
          $nPos = strpos($cFld,'=');
          if ( $nPos !== false ) {  // [Alias]=[key1,key2,...]
            $cAlias = substr($cFld,0,$nPos);
            $cFld = substr($cFld,$nPos+1);
          } else {                  // [key1]=[key1,key2,...]
            $nPos = strpos($cFld,',');
            $cAlias = ($nPos === false ? $cFld : substr($cFld,0,$nPos));
          }
          $this->_SortNameList[$cAlias] = $cFld;
        }
      }
    }
    return true;
  } else {
    //$Result = join(';',$this->_SortNameList);
    $Result = '';
    foreach ( $this->_SortNameList as $key => $value) {
      $Result .= ";$key=$value";
    }
    if ($this->_SortAllFields  ) {
      $Result = '*'.$Result;
    } else {
      $Result = substr($Result,1);    // remove first character -- ';'
    }
    return $Result;
  }
}

// property FieldNameList: TStringList read FFieldNameList;
function FieldNameList ( ) { return $this->_FieldNameList; }

// property AliasNameList: TStringList read FAliasNameList;
function AliasNameList ( ) { return $this->_AliasNameList; }

// property FieldTypeList: TStringList read FFieldTypeList;
function FieldTypeList ( ) { return $this->_FieldTypeList; }

// property FieldTitleList: TStringList read FFieldTitleList;
function FieldTitleList ( ) { return $this->_FieldTitleList; }

// property KeyNameList: TStringList read FKeyNameList;
function KeyNameList ( ) { return $this->_KeyNameList; }

// property SortNameList: TStringList read FSortNameList;
function SortNameList ( ) { return $this->_SortNameList; }

// property KeyFirstList: TStringList read get_KeyFirstList;
function KeyFirstList ( )
{
  if ( count($this->_dataset) > 0 ) {
    $n = $this->_CurrPtr;
    $this->FirstRecord();
    $this->_fetch_current_key_($this->_KeyFirstList);
    $this->_CurrPtr = $n;
  }
  return $this->_KeyFirstList;
}

// property KeyLastList: TStringList read get_KeyLastList;
function KeyLastList ( )
{
  if ( count($this->_dataset) > 0 ) {
    $n = $this->_CurrPtr;
    $this->LastRecord();
    $this->_fetch_current_key_($this->_KeyLastList);
    $this->_CurrPtr = $n;
  }
  return $this->_KeyLastList;
}

// property KeyCurrList: TStringList read get_KeyCurrList;
function KeyCurrList ( )
{
  if ( count($this->_dataset) > 0 ) {
    $this->_fetch_current_key_($this->_KeyCurrList);
  }
  return $this->_KeyCurrList;
}

// property SQL_CurrPage: string read FSQLCurrPage write FSQLCurrPage stored false;
function _SQL_CurrPage ( $cSQL=null )
{
  if ( isset($cSQL) ) {
    $this->_SQLCurrPage = $cSQL;
    return true;
  } else {
    return $this->_SQLCurrPage;
  }
}

//
// property SQL_FirstPage: string read get_SQL_FirstPage;
//
// 輸出讀取第一頁SQL命令:
// 'SELECT [FieldName] FROM [TableName] WHERE [FilterExp]
//    ORDER BY [KeyName] LIMIT [PageSize]'
//
function _SQL_FirstPage ( $nFrom=0 )
{
  return $this->_modify_limit_query_($this->_SQL_Select_().$this->_SQL_Where_().
         $this->_SQL_Order_(),$nFrom,$this->_PageSize);
}

//
// property SQL_PrevPage: string read get_SQL_PrevPage;
//
// 輸出讀取前頁SQL命令:
// 'SELECT [FieldName] FROM [TableName] WHERE [FilterExp AND (Key < [KeyFirst])]
//    ORDER BY [KeyName] DESC LIMIT [PageSize]'
//
function _SQL_PrevPage ( )
{
  return $this->_modify_limit_query_($this->_SQL_Select_().$this->_SQL_Where_('<',$this->KeyFirstList()).
         $this->_SQL_Order_(false),0,$this->_PageSize);
}

//
// property SQL_NextPage: string read get_SQL_NextPage;
//
// 輸出讀取下頁SQL命令:
// 'SELECT [FieldName] FROM [TableName] WHERE [FilterExp AND (Key > [KeyLast])]
//    ORDER BY [KeyName] LIMIT [PageSize]'
//
function _SQL_NextPage ( )
{
  return $this->_modify_limit_query_($this->_SQL_Select_().$this->_SQL_Where_('>',$this->KeyLastList()).
         $this->_SQL_Order_(),0,$this->_PageSize);
}

//
// property SQL_LastPage: string read get_SQL_LastPage;
//
// 輸出讀取最後一頁SQL命令:
// 'SELECT [FieldName] FROM [TableName] WHERE [FilterExp]
//    ORDER BY [KeyName] DESC LIMIT [n]'
//
function _SQL_LastPage ( )
{
  $n = $this->_PageSize > 0
       ? $this->RecordCount()-($this->PageCount()-1)*$this->_PageSize
       : 0;
  return $this->_modify_limit_query_($this->_SQL_Select_().$this->_SQL_Where_().
         $this->_SQL_Order_(false),0,$n);
}

//
// property SQL_RecordCount: string read get_SQL_RecordCount;
//
// 輸出讀取總記錄數SQL命令: (不需要ORDER及LIMIT語法)
// 'SELECT COUNT(*) FROM [TableName] WHERE [FilterExp]'
//
function _SQL_RecordCount ( )
{
  return 'SELECT COUNT(*) FROM '.$this->_TableName.$this->_SQL_Where_();
}

//
// procedure Search_Data(const cWhere, cExp: string; isForward: boolean=true);
//
//
// 搜尋某筆資料 --
//   從頭/尾找起: cWhere = 'WHERE [FilterExp]' 或 空字串
//   從下筆 找起: cWhere = 'WHERE [FilterExp] AND (Key > [目前的鍵值])'
//                    或   'WHERE '(Key > [目前的鍵值])'
//   從上筆 找起: cWhere = 'WHERE [FilterExp] AND (Key < [目前的鍵值])'
//                    或   'WHERE '(Key < [目前的鍵值])'
//   cExp: "[table.欄位] = '%[值]%'" 或 "[table.欄位] = [值]"
//
// 1.SELECT [FieldName] FROM [TableName] [cWhere] AND [cExp]
//     ORDER BY [KeyName] LIMIT 1
// 2.取出找到記錄的鍵值存於TStringList xList中
// 3.SELECT COUNT(*) FROM [TableName] WHERE [FilterExp] AND (Key < [xList])
// 4.得到之前有多少筆, 計算該筆資料在某一頁的第幾筆
// 5.呼叫GotoPage()取得該頁並顯示之
// 6.移動cdsDBM指標, 使之指到正確資料位置(第幾筆)
//
function Search_Data ( $cWhere, $cExp, $isForward=true )
{
  if ( empty($cWhere) ) {
    $cW = empty($cExp) ? '' : ' WHERE '.$cExp;
  } else {
    $cW = empty($cExp) ? $cWhere : $cWhere.' AND '.$cExp;
  }
  $cSQL = $this->_modify_limit_query_($this->_SQL_Select_().$cW.
            $this->_SQL_Order_($isForward),0,1);
  $row = $this->GetQueryV($cSQL);
  $this->_CurrentPage = 0;                // 強迫GotoPage()重新讀取資料
  if ( is_array($row) && count($row) > 0 ) {
    $cnt = count($this->_KeyNameList);
    $xList = array();
    for ( $i = 0 ; $i < $cnt ; $i++ ) {
      $xList[] = $row[0][$this->ToKeyAlias($i)];
    }

    $cSQL = 'SELECT COUNT(*) FROM '.$this->_TableName.$this->_SQL_Where_('<',$xList);
    $nth = $this->GetQueryI($cSQL);       // 之前有多少筆
    if ( ($this->_PageSize > 0) and ($nth >= $this->_PageSize) ) {
      if ( ($this->_RecordCount > 0) and ($nth+1 >= $this->_RecordCount) ) {
        $this->LastPage();
        $i = 0;
      } else {
        if ( $this->_hasOffset ) {
          $this->GotoPage((int)(($nth+$this->_PageSize)/$this->_PageSize)); // 不可再減一
          $i = $nth % $this->_PageSize;
        } else {
          $cSQL = $this->_modify_limit_query_($this->_SQL_Select_().$this->_SQL_Where_('>=',$xList).
                                   $this->_SQL_Order_(),0,$this->_PageSize);
          $this->_new_page_((int)(($nth+$this->_PageSize)/$this->_PageSize),$cSQL);
          $i = 0;
        }
      }
    } else {
      $this->FirstPage();
      $i = $nth;
    }
    if ( $i > 0 ) $this->GotoRecord($i);
    return true;
  }
  //$this->_ERROR_("SearchData('$cExp') -> 無符合資料",$this->_DebugMode);
  return false;
}

/****************************************************************************
 * public: (II) Properties & Functions
 ****************************************************************************/

// property isAscending: boolean read FisAscending write FisAscending;
function isAscending ( $bisAscending=null )
{
  if ( isset($bisAscending) ) {
    settype($bisAscending,'boolean');
    if ( $this->_isAscending != $bisAscending ) {
      $this->_isAscending = $bisAscending;
    /*---- 頁次/順序皆已不正確, 使用者必須執行RefreshPage()重新顯示資料
      $this->_ReversedRecord = !$this->_ReversedRecord;
      $this->_KeyFirstList = array();
      $this->_KeyLastList = array();
      $this->_SQLCurrPage = '';
     */
    }
    return true;
  } else {
    return $this->_isAscending;
  }
}

// property Timeout: integer read FTimeout write FTimeout;
function Timeout ( $nTimeout )
{
  if ( isset($nTimeout) ) {
    $this->_Timeout = ($nTimeout < 5) ? 5 : (int)$nTimeout;
    return true;
  } else {
    return $this->_Timeout;
  }
}

// property PageSize: integer read FPageSize write FPageSize default 15;
function PageSize ( $nPageSize=null )
{
  if ( isset($nPageSize) ) {
    $this->_PageSize = (int)$nPageSize;
    $this->_PageCount = 0;
    return true;
  } else {
    return $this->_PageSize;
  }
}

// property CurrentPage: integer read FCurrentPage;
function CurrentPage ( )
{
  return $this->_CurrentPage;
}

// property PageCount: integer read get_PageCount;
function PageCount ( )
{
  $RC = $this->RecordCount();
  if ( $this->_PageSize > 0 ) {
    if ( ($RC <= 0) || ($this->_PageCount <= 0) ) {
      $this->_PageCount = (int)(($RC+$this->_PageSize-1)/$this->_PageSize);
    }
  } else {
    $this->_PageCount = $RC > 0 ? 1 : 0;
  }
  return $this->_PageCount;
}

//
// property RecordCount: integer read get_RecordCount;
//
// 經由SQL 'SELECT COUNT(*) FROM table WHERE ...' 讀取總筆數, 但為了效能
// 之故, 僅當FRecordCount小於1時才送出SQL命令。若要強迫讀取時, 必須先將
// FRecordCount改為小於或等於0。
//
function RecordCount ( )
{
  if ( $this->_RecordCount <= 0 || time() >= $this->_TimerRecount ) {
    $this->_RecordCount = $this->GetQueryI($this->_SQL_RecordCount());
    $this->_TimerRecount = time() + $this->_Timeout;
  }
  return $this->_RecordCount;
}

// procedure GotoPage(nPage: integer); virtual;
function GotoPage ( $nPage )
{
  if ( $nPage == 1 ) {
    $this->FirstPage();
  } elseif ( $nPage <= 0 || $nPage >= $this->PageCount() ) {
    $this->LastPage();
  } else {
    if ( $this->_hasOffset ) {
      $cSQL = $this->_SQL_FirstPage(($nPage-1)*$this->_PageSize);
      $this->_ReversedRecord = false;
      $this->_new_page_($nPage,$cSQL);
    } else {
      $this->_ERROR_('Not support GotoPage for ODBC',$this->_DebugMode);
    }
  }
}

// procedure FirstPage(isRefresh: boolean=false); virtual;
function FirstPage ( $isRefresh=false )
{
  if ( $isRefresh || ($this->_CurrentPage <> 1) || $this->_need_refresh_() ) {
    $cSQL = $this->_SQL_FirstPage();
    $this->_ReversedRecord = false;
    $this->_new_page_(1,$cSQL);
  }
}

// procedure PrevPage; virtual;
function PrevPage ( )
{
  if ( $this->CurrentPage() > 2 ) {
    $cSQL = $this->_SQL_PrevPage();
    $this->_ReversedRecord = true;
    $this->_new_page_($this->_CurrentPage-1,$cSQL);
  } else {
    $this->FirstPage();
  }
}

// procedure NextPage; virtual;
function NextPage ( )
{
  if ( $this->CurrentPage()+1 < $this->PageCount() ) {
    $cSQL = $this->_SQL_NextPage();
    $this->_ReversedRecord = false;
    $this->_new_page_($this->_CurrentPage+1,$cSQL);
  } else {
    $this->LastPage();
  }
}

// procedure LastPage(lastData: boolean=false); virtual;
function LastPage ( )
{
  if ( $this->_need_refresh_() || $this->CurrentPage() < $this->PageCount() ) {
    if ( $this->PageCount() > 1 ) {
      $cSQL = $this->_SQL_LastPage();
      $this->_ReversedRecord = true;
      $this->_new_page_($this->PageCount(),$cSQL);
    } else {
      $this->FirstPage();
    }
  }
}

// procedure RefreshPage; virtual;
function RefreshPage ( )
{
  return $this->SearchCurr();
}

// procedure SearchFirst(const cFieldExp: string); virtual;
function SearchFirst ( $cFieldExp )
{
  return $this->Search_Data($this->_SQL_Where_(),$cFieldExp);
}

// procedure SearchNext(const cFieldExp: string); virtual;
function SearchNext ( $cFieldExp )
{
  return $this->Search_Data($this->_SQL_Where_('>',$this->KeyCurrList()),$cFieldExp);
}

// procedure SearchPrev(const cFieldExp: string); virtual;
function SearchPrev ( $cFieldExp )
{
  return $this->Search_Data($this->_SQL_Where_('<',$this->KeyCurrList()),$cFieldExp,false);
}

// procedure SearchLast(const cFieldExp: string); virtual;
function SearchLast ( $cFieldExp )
{
  return $this->Search_Data($this->_SQL_Where_(),$cFieldExp,false);
}

//
// procedure SearchCurrent(); virtual;
//
// 改變KeyName('Order By'的順序)或切換isAscending, 必須根據目前Key的值找出
// 在新順序的那一頁。若目前Key為空的則使用第一筆的Key, 若Key均為空的就直接
// 跳到第一頁去。
//
function SearchCurr ( )
{
  $xList = $this->KeyCurrList();
  if ( isEmptyList($xList) ) {
    $xList = $this->KeyFirstList();
    if ( isEmptyList($xList) ) return $this->FirstPage();
  }
  return $this->Search_Data($this->_SQL_Where_('=',$xList),'');
}

// procedure SetFilter(const cFilterExp: string); virtual;
function SetFilter ( $cFilterExp )
{
  if ( $cFilterExp <> $this->_FilterExp ) {
    $this->FilterExp($cFilterExp);
    if (!$this->SearchCurr()) $this->FirstPage();
  }
}

// procedure DBMGridTitleBtnClick(Sender: TObject; ACol: Integer; Field: TField);
function SortBy ( $cField )
{
  $cField = $this->_FixCase_($cField);
  $cValue = '';
  if ( empty($this->_SortNameList) ) {
    if ( $this->_SortAllFields )
      $cValue = empty($this->_PrimaryKeyStr) ? $cField : $cField.','.$this->_PrimaryKeyStr;
  } else {
    $cValue = $this->_SortNameList[$cField];
    if ( empty($cVAlue) && $this->_SortAllFields )
      $cValue = empty($this->_PrimaryKeyStr) ? $cField : $cField.','.$this->_PrimaryKeyStr;
  }
  if ( empty($cValue) ) return false;

  $this->KeyName($cValue);
  return $this->SearchCurr();
}

}

?>
