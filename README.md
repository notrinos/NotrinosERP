[<img src="https://github.com/notrinos/NotrinosERP/raw/master/themes/default/images/notrinos_erp.jpg" width="350" />](http://notrinos.com)
====================

NotrinosERP is an open source, web-based enterprise management system that written in PHP and MySql. NotrinosERP contains all the required modules for running any small to medium size businesses. It supports multi users, multi currencies, multi languages
## [DEMO](http://demo.notrinos.com/erp)

![ScreenShot](https://a.fsdn.com/con/app/proj/notrinos-erp/screenshots/Untitled.png)
![ScreenShot](https://a.fsdn.com/con/app/proj/notrinos-erp/screenshots/gl_dashboard.png)

## Requirements
- HTTP web server - Ex. Apache, Nginx, IIS
- PHP version from 5.0 upto 7.3 (version 5.6 or 7.3 is recommended)
- MySQL version 4.1 and above with Innodb tables enabled, or any MariaDB version
- A web browser with HTML5 compatibility

## Installation
### Manual installation
1. [Download the latest official NotrinosERP snapshot.](https://gitlab.com/aodieu/NotrinosERP/-/archive/master/NotrinosERP-master.zip)
2. Unzip the downloaded file.
3. Everything inside the folder you unzipped needs to be uploaded/copied to your webserver, for example, into your `public_html` or `www` or `html` or `htdocs` folder (the folder will already exist on your webserver).
4. In your browser, enter the address to your site, such as: www.yourdomain.com (or if you uploaded it into another subdirectory such as NotrinosERP use www.yourdomain.com/NotrinosERP)
5. Follow the instructions that appear in your browser for installation.
6. After successful installation please remove `install` folder for safety reasons. You won't need it any more.

### Composer
Run this command in an empty location that you want NotrinosERP to be installed in:  
`composer require notrinos/notrinos-erp`

## Troubleshooting
[Read the wiki](http://support.notrinos.com/ERP/index.php?n=Help.Help)  
If you encountered any problems with NotrinosERP configuration or usage, please consult your case with other users on [NotrinosERP forum](http://forums.notrinos.com).