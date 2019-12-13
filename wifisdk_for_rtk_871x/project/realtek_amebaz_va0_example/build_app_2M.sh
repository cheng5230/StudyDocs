#!/bin/sh
#BUILD_TYPE=8710_1M
BUILD_TYPE=8710_2M

if [ -z "$1" ];then
        echo "please input the parameter file name!!!"
        exit 1
else
        PARA_FILE=$1
fi
k=0
while read line;do
        echo "Line # $k: $line"
		if [ 4 -eq $k ];then
		        break
		elif [ 1 -eq $k ];then
        		PROJ_SRC_PATH=$line
        		echo "PROJ_SRC_PATH = $PROJ_SRC_PATH"
		elif [ 2 -eq $k ];then
        		USER_SW_VER=$line
        		echo "USER_SW_VER = $USER_SW_VER"
		elif [ 3 -eq $k ];then
        		APP_BIN_NAME=$line
        		echo "APP_BIN_NAME = $APP_BIN_NAME"        		
		fi 						     
		((k++))
done < $PARA_FILE

if [ 4 -ne $k ];then
        echo "parameter num=$k < 4!!!"
        exit 1
fi
if [ -z "$APP_BIN_NAME" ];then
        echo "please input the app bin name(no suffix \".bin\")!!!"
        exit 1
fi

if [ -z "$USER_SW_VER" ];then
        echo "please input the app sw version(for example:the format is "1.1.1")!!!"
        exit 1
fi
if [ -z "$PROJ_SRC_PATH" ];then
        echo "please input the project src path!!!"
        exit 1
fi

#mkdir tuya_user/$APP_BIN_NAME/output/$USER_SW_VER
# $3 compile parameter command as user set,for example clean and so on.
USER_DEF_CMD=$2

echo ""
echo "start..."
echo ""
set -e

if [ "$USER_DEF_CMD" = "package" ];then
	echo "do package"
	#ota1
	make APP_BIN_NAME=$APP_BIN_NAME USER_SW_VER=$USER_SW_VER PROJ_SRC_PATH=$PROJ_SRC_PATH BUILD_TYPE=$BUILD_TYPE -C ./GCC-RELEASE
	echo "1111111111111111111111111111111111111111111111111111111111111111111"
	#ota2
	make APP_BIN_NAME=$APP_BIN_NAME USER_SW_VER=$USER_SW_VER PROJ_SRC_PATH=$PROJ_SRC_PATH BUILD_TYPE=$BUILD_TYPE ota_idx=2 -C ./GCC-RELEASE
	echo "2222222222222222222222222222222222222222222222222222222222222222222"
	#mp
	make APP_BIN_NAME=$APP_BIN_NAME USER_SW_VER=$USER_SW_VER PROJ_SRC_PATH=$PROJ_SRC_PATH BUILD_TYPE=$BUILD_TYPE mp -C ./GCC-RELEASE
	echo "3333333333333333333333333333333333333333333333333333333333333333333"
	#do package
	#BIN_PATH=tuya_user/$PROJ_SRC_PATH/output/$USER_SW_VER
	BIN_QIO=tuya_user/$PROJ_SRC_PATH/output/${APP_BIN_NAME}_$USER_SW_VER/${APP_BIN_NAME}_target_QIO_$USER_SW_VER.bin
	BIN_DOUT=tuya_user/$PROJ_SRC_PATH/output/${APP_BIN_NAME}_$USER_SW_VER/${APP_BIN_NAME}_target_DOUT_$USER_SW_VER.bin
	BIN2=GCC-RELEASE/boot_all.bin
	BIN4=tuya_user/$PROJ_SRC_PATH/output/${APP_BIN_NAME}_$USER_SW_VER/mp_${APP_BIN_NAME}_\(1\)_$USER_SW_VER.bin
	BIN5=tuya_user/$PROJ_SRC_PATH/output/${APP_BIN_NAME}_$USER_SW_VER/${APP_BIN_NAME}_\(2\)_$USER_SW_VER.bin
	
	./package_2M $BIN_QIO $BIN2 GCC-RELEASE/system_qio_2M.bin $BIN4 $BIN5
	echo "package_2M QIO file success"
	
	./package_2M $BIN_DOUT $BIN2 GCC-RELEASE/system_dout_2M.bin $BIN4 $BIN5
	echo "package DOUT file success"
	chmod -R 777 tuya_user/$PROJ_SRC_PATH/output/${APP_BIN_NAME}_$USER_SW_VER/
	
	
else
	make APP_BIN_NAME=$APP_BIN_NAME USER_SW_VER=$USER_SW_VER $USER_DEF_CMD PROJ_SRC_PATH=$PROJ_SRC_PATH BUILD_TYPE=$BUILD_TYPE -C ./GCC-RELEASE
	if [ -z "$2" ];then
			make APP_BIN_NAME=$APP_BIN_NAME USER_SW_VER=$USER_SW_VER PROJ_SRC_PATH=$PROJ_SRC_PATH BUILD_TYPE=$BUILD_TYPE ota_idx=2 -C ./GCC-RELEASE
	fi
	echo ""
	echo "***************************************COMPILE SUCCESS!*******************************************"
fi


