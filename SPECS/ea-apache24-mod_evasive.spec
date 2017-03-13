Name:           ea-apache24-mod_evasive
Version:        1.10.1
%define 		release_prefix 1
Release: 		%{release_prefix}%{?dist}.cpanel
Summary:        Denial of Service evasion module for Apache
License:        GPL-2.0+
Group:          Productivity/Networking/Web/Servers
URL:            http://tn123.ath.cx/mod_xsendfile/
Source0:        https://github.com/nmaier/mod_xsendfile/archive/0.12.tar.gz
Source1:		300-mod_xsendfile.conf
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:  ea-apache24-devel
Requires:       ea-apache24 ea-apache24-devel

Url:            http://zdziarski.com/blog/?page_id=442
Source:         http://zdziarski.com/blog/wp-content/uploads/2010/02/mod_evasive_%version.tar.gz
Source2:        mod_evasive.conf
Patch1:         modev-return.diff
Patch2:         mail-invocation.diff
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
BuildRequires:  apache-rpm-macros
BuildRequires:  apache2-devel
BuildRequires:  apache2-prefork
BuildRequires:  curl-devel
BuildRequires:  gcc-c++
BuildRequires:  pcre-devel
Recommends:     mailx
Requires:       %{apache_mmn}
Requires:       %{apache_suse_maintenance_mmn}
Requires:       apache2

%description
mod_evasive is an evasive maneuvers module for Apache to provide
evasive action in the event of an HTTP DoS or DDoS attack or brute
force attack. It is also designed to be a detection and network
management tool, and can be easily configured to talk to ipchains,
firewalls, routers, and etcetera. mod_evasive presently reports
abuses via email and syslog facilities.

%prep
%setup -qn mod_evasive

%build



%endif
/usr/bin/apsx -c mod_evasive.c

%install
b="%"
mkdir -p "%{BUILDROOT}/%apache_libexecdir" "%{BUILDROOT}/%apache_sysconfdir/conf.d"

%apache_apxs -i -S LIBEXECDIR="%{BUILDROOT}/%apache_libexecdir" \
	-n mod_evasive%{ap_suffix}.so mod_evasive.la;
cp -a mod_evasive.conf "$b/%apache_sysconfdir/conf.d/";
perl -i -pe "s{/usr/lib/}{%_libdir/}g" \
	"$b/%apache_sysconfdir/conf.d/mod_evasive.conf";

%check
set +x
%apache_test_module_load -m evasive%{ap_suffix} -i mod_evasive.conf
set -x

%files
%defattr(-,root,root)
%apache_libexecdir/
%config(noreplace) %apache_sysconfdir/conf.d/mod_evasive.conf
%doc CHANGELOG LICENSE README test.pl

%changelog