<?php
define('SS_FILE', '/data/www/base/xsl/mag.xsl');
$ss = fastxsl_parse_stylesheet_file(SS_FILE);
$xd = fastxsl_parse_xml_file('xsl/editorial.xml');
$res = fastxsl_transform($ss, $xd);
echo fastxsl_result_as_string($ss, $res);
?>
