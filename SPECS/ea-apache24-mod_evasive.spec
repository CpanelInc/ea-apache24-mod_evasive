Name:           ea-apache24-mod_evasive
Version:        1.10.1
# Doing release_prefix this way for Release allows for OBS-proof versioning, See EA-4544 for more details
%define         release_prefix 6
Release:        %{release_prefix}%{?dist}.cpanel
Vendor:         cPanel, Inc.
Summary:        Denial of Service evasion module for Apache
License:        GPL-2.0+
Group:          Productivity/Networking/Web/Servers
URL:            https://github.com/jzdziarski/mod_evasive
Requires:       ea-apache24 ea-apache24-devel
Source:         https://github.com/shivaas/mod_evasive/mod_evasive.tar.gz
Source1:        300-mod_evasive.conf
Source2:        300-mod_evasive.modules.conf
Source3:        generate_mod_evasive_local_ips_conf.pl
Patch1:         0001-Make-the-response-to-a-blocked-HTTP-request-configur.patch
BuildRoot: 	%{_tmppath}/%{name}-%{version}-%{release}-root
BuildRequires:  ea-apache24-devel
BuildRequires:  curl-devel
BuildRequires:  pcre-devel
AutoReq:        0

%description
mod_evasive is an evasive maneuvers module for Apache to provide
evasive action in the event of an HTTP DoS or DDoS attack or brute
force attack. It is also designed to be a detection and network
management tool, and can be easily configured to talk to ipchains,
firewalls, routers, and etcetera. mod_evasive presently reports
abuses via email and syslog facilities.

%prep
%setup -q

%patch1 -p1 -b .custom_http_response

%build
cp mod_evasive{20,24}.c
sed 's/connection->remote_ip/connection->client_ip/' < mod_evasive20.c > mod_evasive24.c
sed -i 's/evasive20_module/evasive24_module/' mod_evasive24.c
/usr/bin/apxs -Wc,"%{optflags}" -c mod_evasive24.c

%install
[ "%{buildroot}" != "/" ] && rm -rf %{buildroot}
mkdir -p %{buildroot}/var/log/apache2/mod_evasive
mkdir -p %{buildroot}%{_httpd_confdir}
mkdir -p %{buildroot}%{_httpd_modconfdir}
mkdir -p %{buildroot}%{_httpd_moddir}
install -D %{SOURCE1} %{buildroot}%{_httpd_confdir}/300-mod_evasive.conf
install -D %{SOURCE2} %{buildroot}%{_httpd_modconfdir}/300-mod_evasive.conf

install -pm 755 .libs/mod_evasive24.so $RPM_BUILD_ROOT%{_libdir}/apache2/modules/

mkdir -p $RPM_BUILD_ROOT/usr/local/cpanel/scripts
install -pm 755 %{SOURCE3} $RPM_BUILD_ROOT/usr/local/cpanel/scripts/generate_mod_evasive_local_ips_conf.pl

%files
%defattr(-,root,root)
%config %{_httpd_confdir}/300-mod_evasive.conf
%config(noreplace) %{_httpd_modconfdir}/300-mod_evasive.conf
%attr(0770,root,nobody) %dir /var/log/apache2/mod_evasive
%attr(0755,root,nobody) %{_httpd_moddir}/mod_evasive24.so
%attr(0755,root,root) /usr/local/cpanel/scripts/generate_mod_evasive_local_ips_conf.pl

%post
if [ $1 == 1 ]; then
    /usr/local/cpanel/scripts/generate_mod_evasive_local_ips_conf.pl %{_httpd_confdir}/300-mod_evasive_local_ips.conf
fi

%postun
if [ $1 == 0 ]; then
    rm %{_httpd_confdir}/300-mod_evasive_local_ips.conf
fi

%changelog
* Fri Jul 09 2021 Tim Mullin <tim@cpanel.net> - 1.10.1-6
- EA-9924: Make mod_evasive error code configurable

* Tue Sep 11 2018 Tim Mullin <tim@cpanel.net> - 1.10.1-5
- EA-7330: Automatically add local IPs to DOSWhitelist

* Fri Jan 19 2018 Jacob Perkins <jacob.perkins@cpanel.net> - 1.10.1-4
- EA-7126: Raised default limits to ensure larger bursts of requests can occur without blocking

* Tue Dec 05 2017 Jacob Perkins <jacob.perkins@cpanel.net> - 1.10.1-3
- EA-7005: Fix URL to point to the proper upstream repository

* Thu Oct 26 2017 Dan Muey <dan@cpanel.net> - 1.10.1-2
- EA-6174: Promote from experimental repo to production

* Tue Mar 14 2017 Jacob Perkins <jacob.perkins@cpanel.net> - 1.10.1-1
- Initial commit
