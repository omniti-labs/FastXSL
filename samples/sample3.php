<?php 
fastxsl_parse_stylesheet_file_cached('sample.xsl');
$xd = fastxsl_parse_xml_file('sample.xml');
$res = fastxsl_transform_cached('sample.xsl', $xd);
fastxsl_write_output_cached('sample.xsl', $res);
?>
