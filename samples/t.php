<?php
define('SS_FILE', '/data/www/base/xsl/mag.xsl');
fastxsl_parse_stylesheet_file_cached(SS_FILE);
$xd = fastxsl_parse_xml_file('xsl/editorial.xml');
$res = fastxsl_transform_cached(SS_FILE, $xd);
echo fastxsl_result_as_string_cached(SS_FILE, $res);
?>
