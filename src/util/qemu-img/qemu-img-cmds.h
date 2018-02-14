

DEF("check", img_check,
"check [-f fmt] filename")

DEF("create", img_create,
"create [-f fmt] [-o options] filename [size]")

DEF("commit", img_commit,
"commit [-f fmt] filename")

DEF("convert", img_convert,
"convert [-c] [-f fmt] [-O output_fmt] [-o options] filename [filename2 [...]] output_filename")

DEF("info", img_info,
"info [-f fmt] filename")

DEF("snapshot", img_snapshot,
"snapshot [-l | -a snapshot | -c snapshot | -d snapshot] filename")
