zlib_dep = dependency('zlib')


gme_sources = [
  'Ay_Apu.cc',
  'Ay_Cpu.cc',
  'Ay_Emu.cc',
  'Blip_Buffer.cc',
  'Classic_Emu.cc',
  'Data_Reader.cc',
  'Dual_Resampler.cc',
  'Effects_Buffer.cc',
  'Fir_Resampler.cc',
  'Gbs_Emu.cc',
  'Gb_Apu.cc',
  'Gb_Cpu.cc',
  'Gb_Oscs.cc',
  'gme.cc',
  'Gme_File.cc',
  'Gym_Emu.cc',
  'Gzip_Reader.cc',
  'Hes_Apu.cc',
  'Hes_Cpu.cc',
  'Hes_Emu.cc',
  'Kss_Cpu.cc',
  'Kss_Emu.cc',
  'Kss_Scc_Apu.cc',
  'M3u_Playlist.cc',
  'Multi_Buffer.cc',
  'Music_Emu.cc',
  'Nes_Apu.cc',
  'Nes_Cpu.cc',
  'Nes_Fme7_Apu.cc',
  'Nes_Namco_Apu.cc',
  'Nes_Oscs.cc',
  'Nes_Vrc6_Apu.cc',
  'Nsfe_Emu.cc',
  'Nsf_Emu.cc',
  'Sap_Apu.cc',
  'Sap_Cpu.cc',
  'Sap_Emu.cc',
  'Sms_Apu.cc',
  'Snes_Spc.cc',
  'Spc_Cpu.cc',
  'Spc_Dsp.cc',
  'Spc_Emu.cc',
  'Spc_Filter.cc',
  'Vgm_Emu.cc',
  'Vgm_Emu_Impl.cc',
  'Ym2413_Emu.cc',
  'Ym2612_Emu.cc',
  'Zlib_Inflater.cc'
]


plugin_sources = [
  'Vfs_File.cc',
  'Audacious_Driver.cc',
  'configure.cc',
  'plugin.cc'
]


if zlib_dep.found()
  shared_module('console',
    gme_sources,
    plugin_sources,
    dependencies: [audacious_dep, zlib_dep],
    install: true,
    install_dir: input_plugin_dir
  )
endif
