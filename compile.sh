#!/bin/bash

function compileMboot(){
	echo "starting compile mBoot..."
	cd $root/uboot
	
	if ! ./build_uboot  ; then 
		echo "Mboot :make failed"; 
		exit 1 ; 
	fi

}

function compileKernel(){
	echo "starting compile Kernel..."
	cd $root/vendor/mstar/kernel/linaro

	./genlink.sh
	cp .config_mooney_SMP_arm_andorid_emmc_nand .config

	if ! make menuconfig; then 
		echo "Kernel :make menuconfig failed"; 
		exit 1 ; 
	fi

	make clean
	if ! make ; then 
		echo "Kernel :make failed"; 
		exit 1 ; 
	fi
}
#编译android时，需要先复制mboot,kernel
#编译完成复制镜像到images/kitkat/avocado目录下。
function compileAndroid(){

	cd $root

	copyMboot

	echo "copy RT_PM.bin to /device/mstar/avocado/images/prebuilts/"
	cp $root/vendor/mstar/supernova/target/dvb.mooney/images/ext4/RT_PM.bin $root/device/mstar/avocado/images/prebuilts/

	copyKernel

	echo "starting copy tee.aes;secure_info_tee.bin;nuttx_config.bin;tvservice.img;tvconfig.img;tvdatabase.img;tvcustomer.img  to out/target/product/avocado/"
	cp $root/vendor/mstar/supernova/target/dvb.mooney/images/ext4/tee.aes $root/out/target/product/avocado/
	cp $root/vendor/mstar/supernova/target/dvb.mooney/images/ext4/secure_info_tee.bin $root/out/target/product/avocado/
	cp $root/vendor/mstar/supernova/target/dvb.mooney/images/ext4/nuttx_config.bin $root/out/target/product/avocado/
	cp $root/vendor/mstar/supernova/target/dvb.mooney/images/ext4/tvservice.img $root/out/target/product/avocado/
	cp $root/vendor/mstar/supernova/target/dvb.mooney/images/ext4/tvconfig.img $root/out/target/product/avocado/
	cp $root/vendor/mstar/supernova/target/dvb.mooney/images/ext4/tvdatabase.img $root/out/target/product/avocado/
	cp $root/vendor/mstar/supernova/target/dvb.mooney/images/ext4/tvcustomer.img $root/out/target/product/avocado/
	
	echo "starting compile Android..."
	source build/envsetup.sh
	lunch aosp_avocado-userdebug
	if ! make ; then 
		echo "Android :make failed"; 
		exit 1 ; 
	fi

	echo "starting copy Image..."
	./development/scripts/releaseimage.sh
	
}

function compileAll(){
	compileMboot
	compileKernel
	compileAndroid
}


if [ -z $1 ]; then
	echo "------------------------------------------"
    echo "usage : $0 command"
    echo "------------------------------------------"
	echo "1.<compile Mboot>"
    echo "usage : $0 mboot"
    echo "2.<compile Kernel>"
    echo "usage : $0 kernel"
    echo "3.<compile Android>"
	echo "4.<compile all>"
	echo "usage : $0 compile_all"
	echo "------------------------------------------"

else
  	root=$(pwd)
	
	echo $root
	if [ ! -d $root ]; then
		echo "path doesn't exist,check out first please."
		exit 1
	else
		case $1 in
			"mboot")
			compileMboot
			;;
			"kernel")
			compileKernel
			;;
			"android")
			compileAndroid
			;;
			"compile_all")
			compileAll
			;;
			*)
				echo "wrong command \"$1\" "

				echo "------------------------------------------"
    			echo "usage : $0 command"
    			echo "------------------------------------------"
				echo "1.<compile Mboot>"
    			echo "usage : $0 mboot"
    			echo "2.<compile Kernel>"
    			echo "usage : $0 kernel"
    			echo "3.<compile Android>"
    			echo "usage : $0 android"
				echo "4.<compile all>"
				echo "usage : $0 compile_all"
				echo "------------------------------------------"
			;;
		esac
			echo "finish "
	fi
fi	

