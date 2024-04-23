Name:           m8c
Version:        1.6.0
Release:        1%{?dist}
Summary:        m8c is a client for Dirtywave M8 music tracker's headless mode

License:        MIT
URL:            https://github.com/laamaa/%{name}
Source0:        https://github.com/laamaa/m8c/archive/refs/tags/v%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  make
BuildRequires:  SDL2-devel
BuildRequires:  libserialport-devel
Requires:       SDL2
Requires:       libserialport

%description
m8c is a client for Dirtywave M8 music tracker's headless mode

%prep
%autosetup


%build
make %{?_smp_mflags}

%install
PREFIX=/usr %make_install


%files
%license LICENSE
%{_bindir}/%{name}



%changelog
* Tue Aug 29 2023 Jonne Kokkonen <jonne.kokkonen@ambientia.fi>
- First m8c package
