<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:fo="http://www.w3.org/1999/XSL/Format"
                version="1.0">

<xsl:preserve-space elements="programlisting,screen" />

<xsl:template name="split">
	<xsl:param name="string" select="''" />
   <xsl:param name="pattern" select="' '" />
   <xsl:choose>
      <xsl:when test="contains($string, $pattern)">
         <xsl:if test="not(starts-with($string, $pattern))">
            <xsl:call-template name="split">
               <xsl:with-param name="string" select="substring-before($string, $pattern)" />
               <xsl:with-param name="pattern" select="$pattern" />
            </xsl:call-template>
         </xsl:if>
         <xsl:call-template name="split">
            <xsl:with-param name="string" select="substring-after($string, $pattern)" />
            <xsl:with-param name="pattern" select="$pattern" />
         </xsl:call-template>
      </xsl:when>
      <xsl:otherwise>
		<p>
			<xsl:attribute name="class">PMCode</xsl:attribute>
			<xsl:value-of select="$string" />
		</p>
      </xsl:otherwise>
   </xsl:choose>
</xsl:template>

<xsl:template match="article">
	<html>
		<xsl:attribute name="xmlms:v">urn:schemas-microsoft-com:vml</xsl:attribute>
		<xsl:attribute name="xmlms:o">urn:schemas-microsoft-com:office:office</xsl:attribute>
		<xsl:attribute name="xmlms:w">urn:schemas-microsoft-com:office:word</xsl:attribute>
		<xsl:attribute name="xmlms">http://www.w3.org/TR/REC-html40</xsl:attribute>
		<head>
			<meta>
				<xsl:attribute name="http-equiv">Content-Type</xsl:attribute>
				<xsl:attribute name="content">text/html; charset=windows-1252</xsl:attribute>
			</meta>
			<meta>
				<xsl:attribute name="name">ProgID</xsl:attribute>
				<xsl:attribute name="content">Word.Document</xsl:attribute>
			</meta>
			<meta>
				<xsl:attribute name="name">Generator</xsl:attribute>
				<xsl:attribute name="content">Microsoft Word 9</xsl:attribute>
			</meta>
			<meta>
				<xsl:attribute name="name">Originator</xsl:attribute>
				<xsl:attribute name="content">Microsoft Word 9</xsl:attribute>
			</meta>
			<title>
				<xsl:value-of select="articleinfo/title" />
			</title>
			<style>

&lt;!--
 /* Font Definitions */
@font-face
	{font-family:Courier;
	panose-1:0 0 0 0 0 0 0 0 0 0;
	mso-font-charset:0;
	mso-generic-font-family:auto;
	mso-font-format:other;
	mso-font-pitch:fixed;
	mso-font-signature:3 0 0 0 1 0;}
 /* Style Definitions */
p.MsoNormal, li.MsoNormal, div.MsoNormal
	{mso-style-parent:"";
	margin:0in;
	margin-bottom:.0001pt;
	mso-pagination:widow-orphan;
	font-size:12.0pt;
	font-family:"Times New Roman";
	mso-fareast-font-family:"Times New Roman";}
a:link, span.MsoHyperlink
	{color:blue;
	text-decoration:underline;
	text-underline:single;}
a:visited, span.MsoHyperlinkFollowed
	{color:purple;
	text-decoration:underline;
	text-underline:single;}
p.PMTextbox, li.PMTextbox, div.PMTextbox
	{mso-style-name:PM_Textbox;
	margin:0in;
	margin-bottom:.0001pt;
	line-height:14.0pt;
	mso-pagination:widow-orphan;
	font-size:9.0pt;
	mso-bidi-font-size:10.0pt;
	font-family:Arial;
	mso-fareast-font-family:"Times New Roman";
	mso-bidi-font-family:"Times New Roman";
	mso-ansi-language:EN-US;
	mso-fareast-language:DE;}
p.PMTextboxHeadline, li.PMTextboxHeadline, div.PMTextboxHeadline
	{mso-style-name:PM_Textbox_Headline;
	mso-style-next:PM_Textbox;
	margin:0in;
	margin-bottom:.0001pt;
	line-height:14.0pt;
	mso-pagination:widow-orphan;
	background:#E0E0E0;
	font-size:10.0pt;
	font-family:Arial;
	mso-fareast-font-family:"Times New Roman";
	mso-bidi-font-family:"Times New Roman";
	mso-ansi-language:EN-US;
	mso-fareast-language:DE;
	font-weight:bold;}
p.PMTable, li.PMTable, div.PMTable
	{mso-style-name:PM_Table;
	mso-style-parent:PM_Textbox;
	margin:0in;
	margin-bottom:.0001pt;
	line-height:14.0pt;
	mso-pagination:widow-orphan;
	font-size:9.0pt;
	mso-bidi-font-size:10.0pt;
	font-family:Arial;
	mso-fareast-font-family:"Times New Roman";
	mso-bidi-font-family:"Times New Roman";
	mso-ansi-language:EN-US;
	mso-fareast-language:DE;}
p.PMText, li.PMText, div.PMText
	{mso-style-name:PM_Text;
	margin:0in;
	margin-bottom:.0001pt;
	text-indent:19.85pt;
	line-height:14.0pt;
	mso-pagination:widow-orphan;
	font-size:10.0pt;
	font-family:"Times New Roman";
	mso-fareast-font-family:"Times New Roman";
	mso-ansi-language:EN-US;
	mso-fareast-language:DE;}
p.PMParagraphHeadline, li.PMParagraphHeadline, div.PMParagraphHeadline
	{mso-style-name:PM_Paragraph_Headline;
	mso-style-parent:PM_Text;
	mso-style-next:PM_Text;
	margin-top:12.0pt;
	margin-right:0in;
	margin-bottom:0in;
	margin-left:0in;
	margin-bottom:.0001pt;
	line-height:14.0pt;
	mso-pagination:widow-orphan;
	font-size:10.0pt;
	font-family:Arial;
	mso-fareast-font-family:"Times New Roman";
	mso-bidi-font-family:"Times New Roman";
	mso-ansi-language:EN-US;
	mso-fareast-language:DE;
	font-weight:bold;}
p.PMSubline2, li.PMSubline2, div.PMSubline2
	{mso-style-name:PM_Subline2;
	mso-style-parent:PM_Text;
	mso-style-next:PM_Text;
	margin-top:0in;
	margin-right:0in;
	margin-bottom:6.0pt;
	margin-left:0in;
	line-height:14.0pt;
	mso-pagination:widow-orphan;
	font-size:11.0pt;
	mso-bidi-font-size:10.0pt;
	font-family:"Times New Roman";
	mso-fareast-font-family:"Times New Roman";
	mso-ansi-language:EN-US;
	mso-fareast-language:DE;}
p.PMSubline1, li.PMSubline1, div.PMSubline1
	{mso-style-name:PM_Subline1;
	mso-style-parent:PM_Subline2;
	mso-style-next:PM_Subline2;
	margin-top:6.0pt;
	margin-right:0in;
	margin-bottom:6.0pt;
	margin-left:0in;
	line-height:14.0pt;
	mso-pagination:widow-orphan;
	font-size:12.0pt;
	mso-bidi-font-size:10.0pt;
	font-family:Arial;
	mso-fareast-font-family:"Times New Roman";
	mso-bidi-font-family:"Times New Roman";
	mso-ansi-language:EN-US;
	mso-fareast-language:DE;}
p.PMHeadline, li.PMHeadline, div.PMHeadline
	{mso-style-name:PM_Headline;
	mso-style-parent:PM_Subline1;
	mso-style-next:PM_Subline1;
	margin-top:6.0pt;
	margin-right:0in;
	margin-bottom:0in;
	margin-left:0in;
	margin-bottom:.0001pt;
	mso-pagination:widow-orphan;
	font-size:18.0pt;
	mso-bidi-font-size:10.0pt;
	font-family:Arial;
	mso-fareast-font-family:"Times New Roman";
	mso-bidi-font-family:"Times New Roman";
	mso-ansi-language:EN-US;
	mso-fareast-language:DE;}
p.PMEnumeration, li.PMEnumeration, div.PMEnumeration
	{mso-style-name:PM_Enumeration;
	mso-style-parent:PM_Text;
	margin-top:0in;
	margin-right:0in;
	margin-bottom:0in;
	margin-left:.25in;
	margin-bottom:.0001pt;
	text-indent:-.25in;
	line-height:14.0pt;
	mso-pagination:widow-orphan;
	mso-list:l1 level1 lfo2;
	tab-stops:list .25in .5in;
	font-size:10.0pt;
	font-family:"Times New Roman";
	mso-fareast-font-family:"Times New Roman";
	mso-ansi-language:EN-US;
	mso-fareast-language:DE;}
p.PMCode, li.PMCode, div.PMCode
	{mso-style-name:PM_Code;
	margin:0in;
	margin-bottom:.0001pt;
	mso-pagination:widow-orphan;
	font-size:8.0pt;
	mso-bidi-font-size:10.0pt;
	font-family:Courier;
	mso-fareast-font-family:"Times New Roman";
	mso-bidi-font-family:"Times New Roman";
	mso-ansi-language:FR;
	mso-fareast-language:DE;}
p.PMAuthor, li.PMAuthor, div.PMAuthor
	{mso-style-name:PM_Author;
	mso-style-parent:PM_Text;
	margin:0in;
	margin-bottom:.0001pt;
	line-height:14.0pt;
	mso-pagination:widow-orphan;
	font-size:10.0pt;
	font-family:"Times New Roman";
	mso-fareast-font-family:"Times New Roman";
	mso-ansi-language:EN-US;
	mso-fareast-language:DE;
	font-weight:bold;
	mso-bidi-font-weight:normal;}
p.PMCaption, li.PMCaption, div.PMCaption
	{mso-style-name:PM_Caption;
	margin:0in;
	margin-bottom:.0001pt;
	line-height:14.0pt;
	mso-pagination:widow-orphan;
	background:yellow;
	font-size:10.0pt;
	font-family:"Times New Roman";
	mso-fareast-font-family:"Times New Roman";
	mso-ansi-language:EN-US;
	mso-fareast-language:DE;}
p.PMLinksLiterature, li.PMLinksLiterature, div.PMLinksLiterature
	{mso-style-name:PM_Links&amp;Literature;
	margin-top:0in;
	margin-right:0in;
	margin-bottom:0in;
	margin-left:.25in;
	margin-bottom:.0001pt;
	text-indent:-.25in;
	line-height:14.0pt;
	mso-pagination:widow-orphan;
	mso-list:l0 level1 lfo4;
	tab-stops:list .25in .5in;
	font-size:9.0pt;
	mso-bidi-font-size:10.0pt;
	font-family:Arial;
	mso-fareast-font-family:"Times New Roman";
	mso-bidi-font-family:"Times New Roman";
	mso-ansi-language:EN-US;
	mso-fareast-language:DE;}
p.PMLinksLiteratureHeadline, li.PMLinksLiteratureHeadline, div.PMLinksLiteratureHeadline
	{mso-style-name:PM_Links&amp;Literature_Headline;
	mso-style-parent:PM_Table;
	mso-style-next:PM_Links&amp;Literature;
	margin-top:12.0pt;
	margin-right:0in;
	margin-bottom:0in;
	margin-left:0in;
	margin-bottom:.0001pt;
	line-height:14.0pt;
	mso-pagination:widow-orphan;
	border:none;
	mso-border-bottom-alt:solid windowtext .5pt;
	padding:0in;
	mso-padding-alt:0in 0in 1.0pt 0in;
	font-size:9.0pt;
	mso-bidi-font-size:10.0pt;
	font-family:Arial;
	mso-fareast-font-family:"Times New Roman";
	mso-bidi-font-family:"Times New Roman";
	mso-ansi-language:EN-US;
	mso-fareast-language:DE;
	font-weight:bold;}
p.PMTableHeadline, li.PMTableHeadline, div.PMTableHeadline
	{mso-style-name:PM_Table_Headline;
	mso-style-parent:PM_Textbox_Headline;
	margin:0in;
	margin-bottom:.0001pt;
	line-height:14.0pt;
	mso-pagination:widow-orphan;
	background:#E0E0E0;
	font-size:9.0pt;
	mso-bidi-font-size:10.0pt;
	font-family:Arial;
	mso-fareast-font-family:"Times New Roman";
	mso-bidi-font-family:"Times New Roman";
	mso-ansi-language:EN-US;
	mso-fareast-language:DE;
	font-weight:bold;}
span.msoIns
	{mso-style-type:export-only;
	mso-style-name:"";
	text-decoration:underline;
	text-underline:single;
	color:teal;}
span.msoDel
	{mso-style-type:export-only;
	mso-style-name:"";
	text-decoration:line-through;
	color:red;}
@page Section1
	{size:595.3pt 841.9pt;
	margin:62.35pt 144.6pt 51.05pt 62.35pt;
	mso-header-margin:35.4pt;
	mso-footer-margin:35.4pt;
	mso-paper-source:0;}
div.Section1
	{page:Section1;}
 /* List Definitions */
@list l0
	{mso-list-id:19821579;
	mso-list-template-ids:1490070958;}
@list l1
	{mso-list-id:173154830;
	mso-list-template-ids:894337238;}
@list l2
    {mso-list-id:569539940;
    mso-list-type:hybrid;
    mso-list-template-ids:-1547897428 -1842208472 68354051 68354053 68354049 68354051 68354053 68354049 68354051 68354053;}
@list l2:level1
    {mso-level-number-format:bullet;
    mso-level-style-link:PM_Links&amp;Literature;
	mso-level-text:;
    mso-level-tab-stop:.5in;
    mso-level-number-position:left;
    text-indent:-.25in;
    font-family:Symbol;}
ol
	{margin-bottom:0in;}
ul
	{margin-bottom:0in;}
--&gt;
			</style>
		</head>
		<body>
			<xsl:attribute name="lang">EN-US</xsl:attribute>
			<xsl:attribute name="style">tab-interval:35.4pt</xsl:attribute>
			<div>
				<xsl:attribute name="class">Section1</xsl:attribute>

				<p>
					<xsl:attribute name="class">PMHeadline</xsl:attribute>
					<xsl:value-of select="articleinfo/title"/>
				</p>
				<p>
					<xsl:attribute name="class">PMSubline1</xsl:attribute>
					<xsl:attribute name="style">line-height:14.0pt;mso-line-height-rule:exactly</xsl:attribute>
					<xsl:value-of select="articleinfo/subtitle"/>
				</p>
				<p>
					<xsl:attribute name="class">PMAuthor</xsl:attribute>
					<xsl:apply-templates select="articleinfo/author"/>
				</p>

				<xsl:apply-templates match="example/para/programlisting,example/para/screen"/>
			</div>
		</body>
	</html>
</xsl:template>

<xsl:template match="articleinfo">
</xsl:template>

<xsl:template match="author">
	<xsl:value-of select="firstname"/><xsl:text> </xsl:text><xsl:value-of select="surname"/>
	<xsl:apply-templates select="email"/>
</xsl:template>

<xsl:template match="email">
	&lt;<xsl:value-of select="."/>&gt;
</xsl:template>

<xsl:template match="epigraph">
	<p>
		<xsl:attribute name="class">PMSubline2</xsl:attribute>
		<xsl:attribute name="style">line-height:14.0pt;mso-line-height-rule:exactly</xsl:attribute>
		<xsl:apply-templates match="para"/>
	</p>
</xsl:template>

<xsl:template match="tip">
	<p>
		<xsl:attribute name="class">PMText</xsl:attribute>
		<xsl:attribute name="style">line-height:14.0pt;mso-line-height-rule:exactly</xsl:attribute>
		<xsl:text>&#160;</xsl:text>
	</p>
	<p>
		<xsl:attribute name="class">PMTextboxHeadline</xsl:attribute>
		<xsl:attribute name="style">line-height:14.0pt;mso-line-height-rule:exactly</xsl:attribute>
		<xsl:value-of select="title"/>
	</p>
	<p>
		<xsl:attribute name="class">PMTextbox</xsl:attribute>
		<xsl:attribute name="style">line-height:14.0pt;mso-line-height-rule:exactly</xsl:attribute>
		<xsl:apply-templates match="para"/>
	</p>
	<p>
		<xsl:attribute name="class">PMText</xsl:attribute>
		<xsl:attribute name="style">line-height:14.0pt;mso-line-height-rule:exactly</xsl:attribute>
		<xsl:text>&#160;</xsl:text>
	</p>
</xsl:template>

<xsl:template match="figure">
	<p>
		<xsl:attribute name="class">PMText</xsl:attribute>
		<xsl:attribute name="style">line-height:14.0pt;mso-line-height-rule:exactly</xsl:attribute>
		<xsl:text>&#160;</xsl:text>
	</p>
	<p>
		<xsl:attribute name="class">PMCaption</xsl:attribute>
		<xsl:attribute name="style">line-height:14.0pt;mso-line-height-rule:exactly</xsl:attribute>
		&gt;&gt;&gt;<xsl:value-of select="graphic/@fileref"/>&lt;&lt;&lt;
	</p>
	<p>
		<xsl:attribute name="class">PMCaption</xsl:attribute>
		<xsl:attribute name="style">line-height:14.0pt;mso-line-height-rule:exactly</xsl:attribute>
		Fig <xsl:number level="any" count="figure" format="1"/>.: <xsl:value-of select="title"/>
	</p>
	<p>
		<xsl:attribute name="class">PMText</xsl:attribute>
		<xsl:attribute name="style">line-height:14.0pt;mso-line-height-rule:exactly</xsl:attribute>
		<xsl:text>&#160;</xsl:text>
	</p>
</xsl:template>

<xsl:template match="section">
	<p>
		<xsl:attribute name="class">PMParagraphHeadline</xsl:attribute>
		<xsl:attribute name="style">line-height:14.0pt;mso-line-height-rule:exactly</xsl:attribute>
		<xsl:value-of select="title"/>
	</p>
	<xsl:apply-templates match="para"/>
</xsl:template>

<xsl:template match="section/title">
</xsl:template>

<xsl:template match="example/title">
</xsl:template>

<xsl:template match="programlisting">
	<p>
		<xsl:attribute name="class">PMText</xsl:attribute>
		<xsl:attribute name="style">line-height:14.0pt;mso-line-height-rule:exactly</xsl:attribute>
		<xsl:text>&#160;</xsl:text>
	</p>
	<p>
		<xsl:attribute name="class">PMCaption</xsl:attribute>
		<xsl:attribute name="style">line-height:14.0pt;mso-line-height-rule:exactly</xsl:attribute>
		Listing <xsl:number level="any" count="programlisting" format="1"/> 
	</p>
	<xsl:call-template name="split">
		<xsl:with-param name="string">
			<xsl:value-of select="translate(.,' ','&#160;')"/>
		</xsl:with-param>
		<xsl:with-param name="pattern">
				<xsl:text>
</xsl:text>
		</xsl:with-param>
	</xsl:call-template>
	<p>
		<xsl:attribute name="class">PMText</xsl:attribute>
		<xsl:attribute name="style">line-height:14.0pt;mso-line-height-rule:exactly</xsl:attribute>
		<xsl:text>&#160;</xsl:text>
	</p>
</xsl:template>

<xsl:template match="screen">
	<p>
		<xsl:attribute name="class">PMText</xsl:attribute>
		<xsl:attribute name="style">line-height:14.0pt;mso-line-height-rule:exactly</xsl:attribute>
		<xsl:text>&#160;</xsl:text>
	</p>
	<xsl:call-template name="split">
		<xsl:with-param name="string">
			<xsl:value-of select="translate(.,' ','&#160;')"/>
		</xsl:with-param>
		<xsl:with-param name="pattern">
				<xsl:text>
</xsl:text>
		</xsl:with-param>
	</xsl:call-template>
	<p>
		<xsl:attribute name="class">PMText</xsl:attribute>
		<xsl:attribute name="style">line-height:14.0pt;mso-line-height-rule:exactly</xsl:attribute>
		<xsl:text>&#160;</xsl:text>
	</p>
</xsl:template>

<xsl:template match="section/para">
	<p>
		<xsl:attribute name="class">PMText</xsl:attribute>
		<xsl:attribute name="style">line-height:14.0pt;mso-line-height-rule:exactly</xsl:attribute>
		<xsl:apply-templates/>
	</p>
</xsl:template>

<xsl:template match="variabele">
	<i><xsl:value-of select="."/></i>
</xsl:template>

<xsl:template match="parameter">
	<i><xsl:value-of select="."/></i>
</xsl:template>

<xsl:template match="literal">
	<i><xsl:value-of select="."/></i>
</xsl:template>

<xsl:template match="function">
	<i><xsl:value-of select="."/>()</i>
</xsl:template>

<xsl:template match="glosslist">
	<p>
		<xsl:attribute name="class">PMLinksLiteratureHeadline</xsl:attribute>
		<xsl:attribute name="style">line-height:14.0pt;mso-line-height-rule:exactly</xsl:attribute>
		Links &amp; Literature
	</p>
	<xsl:apply-templates match="glossentry"/>
</xsl:template>

<xsl:template match="glossentry">
	<p>
		<xsl:attribute name="class">PMLinksLiterature</xsl:attribute>
		<xsl:attribute name="style">margin-left:0in;text-indent:0in;line-height:14.0pt;mso-line-height-rule:exactly;mso-list:l2 level1 lfo5;tab-stops:.25in</xsl:attribute>
		<xsl:value-of select="glossterm"/><xsl:text>: </xsl:text><xsl:value-of select="glossdef/para"/>
	</p>
</xsl:template>


<xsl:template match="itemizedlist">
	<xsl:apply-templates match="listitem"/>
</xsl:template>

<xsl:template match="itemizedlist/listitem">
	<p>
		<xsl:attribute name="class">PMEnumeration</xsl:attribute>
		<xsl:attribute name="style">margin-left:0in;text-indent:0in;line-height:14.0pt;mso-line-height-rule:exactly;mso-list:l2 level1 lfo5;tab-stops:.25in</xsl:attribute>
		<xsl:value-of select="."/>
	</p>
</xsl:template>


<xsl:template match="variablelist">
	<xsl:apply-templates match="varlistentry"/>
</xsl:template>

<xsl:template match="variablelist/varlistentry">
	<p>
		<xsl:attribute name="class">PMEnumeration</xsl:attribute>
		<xsl:attribute name="style">margin-left:0in;text-indent:0in;line-height:14.0pt;mso-line-height-rule:exactly;mso-list:l2 level1 lfo5;tab-stops:.25in</xsl:attribute>
		<xsl:value-of select="term"/>: <xsl:value-of select="listitem"/>
	</p>
</xsl:template>


<xsl:template match="table">
	<p>
		<xsl:attribute name="class">PMText</xsl:attribute>
		<xsl:attribute name="style">line-height:14.0pt;mso-line-height-rule:exactly</xsl:attribute>
		<xsl:text>&#160;</xsl:text>
	</p>
	<p>
		<xsl:attribute name="border">1</xsl:attribute>
		<xsl:attribute name="style">border-collapse:collapse;border:none;mso-border-alt:solid windowtext .5pt;mso-padding-alt:0in 3.5pt 0in 3.5pt</xsl:attribute>
	</p>
	<xsl:apply-templates select="tgroup"/>
	<p>
		<xsl:attribute name="class">PMTableHeadline</xsl:attribute>
		Table <xsl:number level="any" count="table" format="1"/>.
		<xsl:value-of select="title"/>
	</p>
	<p>
		<xsl:attribute name="class">PMText</xsl:attribute>
		<xsl:attribute name="style">line-height:14.0pt;mso-line-height-rule:exactly</xsl:attribute>
		<xsl:text>&#160;</xsl:text>
	</p>
</xsl:template>

<xsl:template match="table/title">
</xsl:template>

<xsl:template match="tgroup">
	<table>
		<xsl:attribute name="border">1</xsl:attribute>
		<xsl:attribute name="cellspacing">0</xsl:attribute>
		<xsl:attribute name="cellpadding">0</xsl:attribute>
		<xsl:apply-templates/>
	</table>
</xsl:template>

<xsl:template match="thead">
	<xsl:apply-templates select="row"/>
</xsl:template>

<xsl:template match="tbody">
	<xsl:apply-templates select="row"/>
</xsl:template>

<xsl:template match="row">
	<tr>
		<xsl:apply-templates match="entry"/>
	</tr>
</xsl:template>

<xsl:template match="thead/row/entry">
	<td>
		<xsl:attribute name="style">padding:0in 3.5pt 0in 3.5pt</xsl:attribute>
		<p>
			<xsl:attribute name="class">PMTable</xsl:attribute>
			<xsl:attribute name="style">line-height:14.0pt;mso-line-height-rule:exactly</xsl:attribute>
			<b><xsl:value-of select="."/></b>
		</p>
	</td>
</xsl:template>

<xsl:template match="tbody/row/entry">
	<td>
		<xsl:attribute name="style">padding:0in 3.5pt 0in 3.5pt</xsl:attribute>
		<p>
			<xsl:attribute name="class">PMTable</xsl:attribute>
			<xsl:attribute name="style">line-height:14.0pt;mso-line-height-rule:exactly</xsl:attribute>
			<xsl:value-of select="."/>
		</p>
	</td>
</xsl:template>

</xsl:stylesheet>

