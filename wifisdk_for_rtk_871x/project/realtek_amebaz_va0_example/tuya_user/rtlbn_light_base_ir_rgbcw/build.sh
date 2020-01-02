#!/bin/sh
if [ -z "$1" ];then
        echo "please input the parameter file name!!!"
        exit 1
else
        PARA_FILE=$1
fi

USER_DEF_CMD=$2

cp $PARA_FILE src/.compile_usr_cfg.h
cp $PARA_FILE include/.compile_usr_cfg.h
usr_prj_src_dir=`pwd`

cd ../../
bash build_app_2M.sh $usr_prj_src_dir/$PARA_FILE $USER_DEF_CMD
cd $usr_prj_src_dir
rm src/.compile_usr_cfg.h -rf