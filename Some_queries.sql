
SELECT  lm.Header_Msg_Code as Header_Code, lm.Msg_Code, lm.Schema, Fields.Tag_Code
FROM  Fields, LoggerMessages lm
WHERE Fields.Msg_Code = lm.Header_Msg_Code
UNION
SELECT  lm.Header_Msg_Code as Header_Code, lm.Msg_Code, lm.Schema, Fields.Tag_Code
FROM Fields, LoggerMessages lm
WHERE Fields.Msg_Code = lm.Msg_Code

CREATE VIEW  Msg_To


-- Access view!!!
CREATE VIEW _Header_Fields
AS
SELECT  lm.Header_Msg_Code AS Header_Code,
        lm.Msg_Code        AS Field_Msg_Code,
        lm.Schema,
        Fields.Tag_Code
FROM  Fields, LoggerMessages lm
WHERE Fields.Msg_Code = lm.Header_Msg_Code
UNION
SELECT  lm.Header_Msg_Code AS Header_Code,
        lm.Msg_Code        AS Field_Msg_Code,
        lm.Schema,
        Fields.Tag_Code
FROM Fields, LoggerMessages lm
WHERE Fields.Msg_Code = lm.Msg_Code


CREATE VIEW Header_Field_name
SELECT DISTINCT Header_Fields.Schema, Header_Fields.Tag_Code, Header_Fields.Header_Msg_Code
FROM {oj Header_Fields Header_Fields LEFT OUTER JOIN Messages Messages_1 ON Header_Fields.Header_Msg_Code = Messages_1.Msg_Code}
WHERE (Header_Fields.Schema='CDCC')



CREATE VIEW  Msg_Tags AS
SELECT Header_Fields.Header_Msg_Code, Header_Fields.Schema, Header_Fields.Tag_Code, Messages.Msg_Code, Messages.Msg_Name, Messages.Msg_Archiving_Name, Messages.Msg_Type, Messages.Msg_Description, Messages.Comment
FROM Header_Fields Header_Fields, Messages Messages
WHERE Messages.Msg_Code = Header_Fields.Msg_Code


microsoft doc on MS_ACCESS (Jet) SQL language
https://blog.pagesd.info/public/2006/fundamental-microsoft-jet-sql-for-access.pdf
https://blog.pagesd.info/public/2006/intermediate-microsoft-jet-sql-for-access.pdf
https://blog.pagesd.info/public/2006/advanced-microsoft-jet-sql-for-access.pdf

create table MsgHeader
(
    name varchar,
    msg_code INTEGER,

)