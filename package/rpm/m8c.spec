Name:           m8c
Version:        2.0.0
Release:        1%{?dist}
Summary:        m8c is a client for Dirtywave M8 music tracker's headless mode

License:        MIT
URL:            https://github.com/laamaa/%{name}
Source0:        https://github.com/laamaa/m8c/archive/refs/tags/v%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  make
BuildRequires:  SDL3-devel
BuildRequires:  libserialport-devel
Requires:       SDL3
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
* Sat Mar 01 2025 Jonne Kokkonen <jonne.kokkonen@gmail.com>
- Update dependencies (SDL2 -> SDL3)
* Tue Aug 29 2023 Jonne Kokkonen <jonne.kokkonen@gmail.com>
- First m8c package
