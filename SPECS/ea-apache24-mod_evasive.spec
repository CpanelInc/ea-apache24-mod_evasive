Name:           ea-apache24-mod_evasive
Version:        1.10.1
# Doing release_prefix this way for Release allows for OBS-proof versioning, See EA-4544 for more details
%define         release_prefix 2
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
BuildRoot: 	%{_tmppath}/%{name}-%{version}-%{release}-root
BuildRequires:  ea-apache24-devel
BuildRequires:  curl-devel
BuildRequires:  pcre-devel

%description
mod_evasive is an evasive maneuvers module for Apache to provide
evasive action in the event of an HTTP DoS or DDoS attack or brute
force attack. It is also designed to be a detection and network
management tool, and can be easily configured to talk to ipchains,
firewalls, routers, and etcetera. mod_evasive presently reports
abuses via email and syslog facilities.

%prep
%setup -q

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

%files
%defattr(-,root,root)
%config %{_httpd_confdir}/300-mod_evasive.conf
%config(noreplace) %{_httpd_modconfdir}/300-mod_evasive.conf
%attr(0770,root,nobody) %dir /var/log/apache2/mod_evasive
%attr(0755,root,nobody) %{_httpd_moddir}/mod_evasive24.so

%changelog
* Tue Dec 05 2017 Jacob Perkins <jacob.perkins@cpanel.net> - 1.10.1-3
- EA-7005: Fix URL to point to the proper upstream repository

* Thu Oct 26 2017 Dan Muey <dan@cpanel.net> - 1.10.1-2
- EA-6174: Promote from experimental repo to production

* Tue Mar 14 2017 Jacob Perkins <jacob.perkins@cpanel.net> - 1.10.1-1
- Initial commit
