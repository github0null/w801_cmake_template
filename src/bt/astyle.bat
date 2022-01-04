REM 批量将本目录中的所有C++文件用Astyle进行代码美化操作
REM 设置Astyle命令位置和参数
@echo off
set astyle="astyle.exe"
REM 循环遍历目录
for /r . %%a in (*.cpp;*.c) do %astyle% --style=allman --convert-tabs --indent=spaces=4 --attach-closing-while --indent-switches --indent-namespaces --indent-continuation=4 --indent-preproc-block --indent-preproc-define --indent-preproc-cond --indent-col1-comments --pad-oper --unpad-paren --delete-empty-lines --align-pointer=name --align-reference=name --break-elseifs --add-braces --break-blocks --indent-preproc-cond -s4 -n "%%a"
for /r . %%a in (*.hpp;*.h) do %astyle% --style=allman --convert-tabs --indent=spaces=4 --attach-closing-while --indent-switches --indent-namespaces --indent-continuation=4 --indent-preproc-block --indent-preproc-define --indent-preproc-cond --indent-col1-comments --pad-oper --unpad-paren --delete-empty-lines --align-pointer=name --align-reference=name --break-elseifs --add-braces --break-blocks --indent-preproc-cond -s4 -n "%%a"
REM 删除所有的astyle生成文件
for /r . %%a in (*.orig) do del "%%a"
pause