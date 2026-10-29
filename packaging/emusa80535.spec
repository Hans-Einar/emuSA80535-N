Name:           emusa80535
Version:        1.0.0
Release:        1%{?dist}
Summary:        Deterministic SAB80535 emulator and headless debug runtime
License:        MIT
URL:            https://github.com/Hans-Einar/emuSA80535-N
Source0:        %{name}-%{version}.tar.gz
BuildRequires:  gcc
BuildRequires:  make
BuildRequires:  binutils
BuildRequires:  python3
BuildRequires:  pkgconf-pkg-config
ExclusiveArch:  x86_64

%description
Generic deterministic 8051/SAB80535 emulation with an emu-debug 1.0 NDJSON
stdio executable. No curses, board model, firmware image or physical I/O is
required. Advanced CPU watchpoint/tracepoint integration is not included.

%package devel
Summary:        Static embedding SDK for emusa80535
Requires:       %{name}%{?_isa} = %{version}-%{release}
Requires:       pkgconf-pkg-config
Provides:       %{name}-static = %{version}-%{release}

%description devel
Headers, static library, pkg-config metadata and an embedding example for
the generic CPU and same-build debugger interfaces. Consumers must rebuild
with matching headers; a stable binary ABI is not promised.

%prep
%autosetup
test "$(wc -l < SOURCE_COMMIT)" -eq 1
grep -Eq '^[0-9a-f]{40}$' SOURCE_COMMIT

%build
%set_build_flags
%make_build emu-debug emu-runtime sdk CFLAGS="$CFLAGS" LDFLAGS="$LDFLAGS" DEBUG_COMMIT="$(cat SOURCE_COMMIT)"

%install
%make_install prefix=%{_prefix} libdir=%{_libdir} DEBUG_COMMIT="$(cat SOURCE_COMMIT)"

%check
%set_build_flags
# core-test includes the CPU API1 facade suite.
%make_build core-test CFLAGS="$CFLAGS"
%make_build debug-test CFLAGS="$CFLAGS" PYTHON=python3 DEBUG_COMMIT="$(cat SOURCE_COMMIT)"
python3 packaging/check_install.py %{buildroot} --libdir %{_libdir} --commit "$(cat SOURCE_COMMIT)"

%files
%license %{_licensedir}/%{name}/LICENSE
%dir %{_docdir}/%{name}
%doc %{_docdir}/%{name}/README.md
%doc %{_docdir}/%{name}/PACKAGING.md
%{_docdir}/%{name}/source-commit
%{_bindir}/emu-debug
%{_bindir}/emu-runtime
%doc %{_docdir}/%{name}/runtime/

%files devel
%{_includedir}/%{name}/
%{_libdir}/libemusa80535.a
%{_libdir}/libemusa80535_cpu.a
%{_libdir}/libemusa80535_runtime.a
%{_libdir}/pkgconfig/emusa80535.pc
%doc %{_docdir}/%{name}/sdk_smoke.c
%doc %{_docdir}/%{name}/interface-bundles/

%changelog
* Mon Oct 26 2026 Hans-Einar <warloc@beinerud> - 1.0.0-1
- Package accepted generic emulator and static embedding SDK for AlmaLinux 10.2.
