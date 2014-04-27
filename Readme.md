# VKAudioFS - FUSE virtual file system for VK (VKontakte) audio records

## Description

VKAudioFS gives you a possibility to mount your audio records as a simple directory is OSX or Linux and use your favourite audio player with them. 

**IMPOTANT**: you are allowed to use this file system only for listening your music in applications. Copying mounted files to another directory violates paragraph 12 of API user agreement published at [http://vk.com/dev/rules](http://vk.com/dev/rules) and may lead to the termination of your account and to the Russian military invasion in your country. 

Please note the filesystem does not handle remote playlist updates. You have to remount FS manually on adding or removing files from your audio list on VK.

Current limitation is 6000 audio files per account.

## Installation

1. Install prerequisites:
    - **OSX**: *brew install pkg-config osxfuse glib json-c*
    - **Debian / Ubuntu**: *sudo apt-get install build-essential git pkg-config fuse libfuse-dev libglib2.0-dev libjson0-dev*
2. Optionally you can change VK_APP_ID in include/config.h to match your application id. You can generate new standalone app here: [http://vk.com/editapp?act=create](http://vk.com/editapp?act=create).
3. Clone repository: *git clone https://github.com/mindcollapse/vkaudiofs.git*
4. Build and install: *cd vkaudiofs && make && make install*

## Usage

At first you need to obtain OAuth token and security ID. Go and get some authentication URL by executing *vkaudiofs --oath*.

Open the link and confirm the permissions request. This will redirect you to blank.html URL. See example below.

https://api.vk.com/blank.html#access_token=LONG_TOKEN_BLABLABLA&expires_in=0&user_id=USER_NUMERIC_ID

Copy LONG_TOKEN_BLABLABLA and USER_NUMERIC_ID parts. 

You are a stone's throw from the end:

1. Create mount point: *mkdir ~/Music/VK*
2. Mount your records: *vkaudiofs ~/Music/VK --access_token LONG_TOKEN_BLABLABLA --user_id USER_NUMERIC_ID*
3. Wait few minutes while it updates sizes for files in your playlist using HTTP HEAD request. We cache all results, so next time this should take less then a second.

![That's all folks](http://i.imgur.com/gXlLvZD.jpg)

## Screenshot

![VKAudiFS screenshot](http://i.imgur.com/xRR8FJO.png)

