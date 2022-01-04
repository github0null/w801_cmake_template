#!/bin/sh

chipType=W800

##########################
# project info
ProjName=$1
ObjFolder=$2

if [ x"$ProjName" == x"" ]; then
  ProjName="${chipType}_demo"
fi

if [ x"$ObjFolder" == x"" ]; then
  ObjFolder=./build/Debug
fi

##########################
# folders
BinFolder=./bin
ToolsFolder=./tools

##########################
# firmware encrypt config
signature=0
prikey_sel=0
code_encrypt=0
sign_pubkey_src=0

##########################
# img addr
sec_img_header=8002000
sec_img_pos=8002400
run_img_header=80d0000
run_img_pos=80d0400
upd_img_pos=8010000

img_type=1
zip_type=1

if [ $prikey_sel -gt 0 ]
 then
  let img_type=$img_type+32*$prikey_sel
fi

if [ $code_encrypt -eq 1 ]
 then
  let img_type=$img_type+16
fi

if [ $signature -eq 1 ]
 then
  let img_type=$img_type+256
fi

if [ $sign_pubkey_src -eq 1 ]
 then
  let img_type=$img_type+512
fi

##################################
# copy bin files

rm -rf ./${BinFolder}
mkdir -p ${BinFolder}

cp -f "${ObjFolder}/${ProjName}.bin" "${BinFolder}/${ProjName}.bin"
cp -f "${ObjFolder}/${ProjName}.elf" "${BinFolder}/${ProjName}.elf"
cp -f "${ObjFolder}/${ProjName}.map" "${BinFolder}/${ProjName}.map"

##################################
# gen firmware

if [ $code_encrypt -eq 1 ]
 then
  let prikey_sel=$prikey_sel+1
  openssl enc -aes-128-ecb -in ${BinFolder}/"$ProjName".bin -out ${BinFolder}/"$ProjName"_enc.bin -K 30313233343536373839616263646566 -iv 01010101010101010101010101010101
  openssl rsautl -encrypt -in ${ToolsFolder}/${chipType}/ca/key.txt -inkey ${ToolsFolder}/${chipType}/ca/capub_"$prikey_sel".pem -pubin -out key_en.dat
  cat ${BinFolder}/"$ProjName"_enc.bin key_en.dat > ${BinFolder}/"$ProjName"_enc_key.bin
  cat ${BinFolder}/"$ProjName"_enc_key.bin ${ToolsFolder}/${chipType}/ca/capub_"$prikey_sel"_N.dat > ${BinFolder}/"$ProjName"_enc_key_N.bin  
  ${ToolsFolder}/${chipType}/wm_tool.exe -b ${BinFolder}/"$ProjName"_enc_key_N.bin -o ${BinFolder}/"$ProjName" -it $img_type -fc 0 -ra $run_img_pos -ih $run_img_header -ua $upd_img_pos -nh 0  -un 0
 else
  ${ToolsFolder}/${chipType}/wm_tool.exe -b ${BinFolder}/"$ProjName".bin -o ${BinFolder}/"$ProjName" -it $img_type -fc 0 -ra $run_img_pos -ih $run_img_header -ua $upd_img_pos -nh 0  -un 0
fi

if [ $signature -eq 1 ]
 then
  openssl dgst -sign  ${ToolsFolder}/${chipType}/ca/cakey.pem -sha1 -out ${BinFolder}/"$ProjName"_sign.dat ${BinFolder}/"$ProjName".img
  cat ${BinFolder}/"$ProjName".img ${BinFolder}/"$ProjName"_sign.dat > ${BinFolder}/"$ProjName"_sign.img
  mv ${BinFolder}/"$ProjName"_sign.img ${BinFolder}/"$ProjName"_sign.img

  #when you change run-area image's ih, you must remake secboot img with secboot img's -nh address same as run-area image's ih
  ${ToolsFolder}/${chipType}/wm_tool.exe -b ${ToolsFolder}/${chipType}/${chipType}_secboot.bin -o ${BinFolder}/"$ProjName"_secboot -it 0 -fc 0 -ra $sec_img_pos -ih $sec_img_header -ua $upd_img_pos -nh $run_img_header  -un 0    
  cat ${BinFolder}/"$ProjName"_secboot.img ${BinFolder}/"$ProjName"_sign.img > ${BinFolder}/"$ProjName".fls  
 else
  #when you change run-area image's ih, you must remake secboot img with secboot img's -nh address same as run-area image's ih
  ${ToolsFolder}/${chipType}/wm_tool.exe -b ${ToolsFolder}/${chipType}/${chipType}_secboot.bin -o ${BinFolder}/"$ProjName"_secboot -it 0 -fc 0 -ra $sec_img_pos -ih $sec_img_header -ua $upd_img_pos -nh $run_img_header  -un 0  
  cat ${BinFolder}/"$ProjName"_secboot.img ${BinFolder}/"$ProjName".img > ${BinFolder}/"$ProjName".fls
fi

#produce compressed ota firmware*/
if [ $zip_type -eq 1 ]
 then
  if [ $signature -eq 1 ]
   then
    ${ToolsFolder}/${chipType}/wm_tool.exe -b ${BinFolder}/"$ProjName"_sign.img -o ${BinFolder}/"$ProjName"_sign -it $img_type -fc 1 -ra $run_img_pos -ih $run_img_header -ua $upd_img_pos -nh 0  -un 0
    mv ${BinFolder}/"$ProjName"_sign_gz.img ${BinFolder}/"$ProjName"_sign_ota.img
  else
   ${ToolsFolder}/${chipType}/wm_tool.exe -b ${BinFolder}/"$ProjName".img -o ${BinFolder}/"$ProjName" -it $img_type -fc 1 -ra $run_img_pos -ih $run_img_header -ua $upd_img_pos -nh 0  -un 0
   mv ${BinFolder}/"$ProjName"_gz.img ${BinFolder}/"$ProjName"_ota.img
  fi
fi
#openssl --help