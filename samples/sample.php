<?php 
fastxsl_parse_stylesheet_file_cached('sample.xsl');
$xd = fastxsl_parse_xml_file('sample.xml');
$res = fastxsl_transform_cached('sample.xsl', $xd);
echo fastxsl_result_as_string_cached('sample.xsl', $res);
?>
