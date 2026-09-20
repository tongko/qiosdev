#pragma once

#include <kernel/devices/device.h>
#include <kernel/devices/blockdev.h>
#include <stdint.h>
#include <stddef.h>

// Simplified ext4 superblock & on-disk structures
typedef struct {
	/* 0x00 */
	uint32_t s_inodes_count;			// Total inode count
	uint32_t s_blocks_count_lo;		// Total block count (low 32 bits)
	uint32_t s_r_blocks_count_lo;		// Reserved blocks count (low 32 bits)
	uint32_t s_free_blocks_count_lo; // Free blocks count (low 32 bits)

	/* 0x10 */
	uint32_t s_free_inodes_count; // Free inodes count
	uint32_t s_first_data_block;	// First Data Block (0 for 4k blocks, 1 for 1k blocks)
	uint32_t s_log_block_size;		// Block size = 1024 << s_log_block_size (e.g., 2 = 4096 bytes)
	uint32_t s_log_cluster_size;	// Cluster size (for bigalloc)

	/* 0x20 */
	uint32_t s_blocks_per_group;	 // Blocks per group
	uint32_t s_clusters_per_group; // Clusters per group
	uint32_t s_inodes_per_group;	 // Inodes per group
	uint32_t s_mtime;					 // Mount time (POSIX timestamp)

	/* 0x30 */
	uint32_t s_wtime;				 // Write time
	uint16_t s_mnt_count;		 // Mount count since last check
	int16_t s_max_mnt_count;	 // Max mount count before check
	uint16_t s_magic;				 // Magic signature (Always 0xEF53)
	uint16_t s_state;				 // File system state (1 = Cleanly unmounted, 2 = Errors detected)
	uint16_t s_errors;			 // Behaviour when detecting errors
	uint16_t s_minor_rev_level; // Minor revision level

	/* 0x40 */
	uint32_t s_lastcheck;	  // Time of last check
	uint32_t s_checkinterval; // Max time between checks
	uint32_t s_creator_os;	  // Creator OS (0 = Linux)
	uint32_t s_rev_level;	  // Revision level (0 = original, 1 = v2 with dynamic inodes)

	/* 0x50 */
	uint16_t s_def_resuid; // Default uid for reserved blocks
	uint16_t s_def_resgid; // Default gid for reserved blocks

	// -- Ext3 / Ext4 Specific Dynamic Fields --
	/* 0x54 */
	uint32_t s_first_ino;			// First non-reserved inode
	uint16_t s_inode_size;			// Size of inode structure (usually 256 bytes for Ext4)
	uint16_t s_block_group_nr;		// Block group # of this superblock
	uint32_t s_feature_compat;		// Compatible feature set flags
	uint32_t s_feature_incompat;	// Incompatible feature set flags
	uint32_t s_feature_ro_compat; // RO compatible feature set flags

	/* 0x6C */
	uint8_t s_uuid[16];		 // 128-bit volume UUID
	char s_volume_name[16];	 // Volume name
	char s_last_mounted[64]; // Directory where last mounted

	/* 0xBC */
	uint32_t s_algorithm_usage_bitmap; // For compression

	// -- Performance Hints --
	uint8_t s_prealloc_blocks;		  // Nr of blocks to try to preallocate
	uint8_t s_prealloc_dir_blocks;  // Nr to preallocate for dirs
	uint16_t s_reserved_gdb_blocks; // Per group table for online growth

	// -- Journaling Support --
	uint8_t s_journal_uuid[16]; // UUID of journal superblock
	uint32_t s_journal_inum;	 // Inode number of journal file
	uint32_t s_journal_dev;		 // Device number of journal file
	uint32_t s_last_orphan;		 // Start of list of inodes to delete

	// -- 64-bit Support (Block counts high 32 bits) --
	uint32_t s_hash_seed[4];	 // HTREE hash seed
	uint8_t s_def_hash_version; // Default hash version to use
	uint8_t s_jnl_backup_type;
	uint16_t s_desc_size; // Size of group descriptor (64 if 64bit enabled)

	uint32_t s_default_mount_opts;
	uint32_t s_first_meta_bg; // First metablock block group
	uint32_t s_mkfs_time;	  // When the filesystem was created

	uint32_t s_jnl_blocks[17]; // Backup of journal inode

	/* 0x150 */
	uint32_t s_blocks_count_hi;		// Total block count (high 32 bits)
	uint32_t s_r_blocks_count_hi;		// Reserved blocks count (high 32 bits)
	uint32_t s_free_blocks_count_hi; // Free blocks count (high 32 bits)

	// ... padding and crypto/checksum fields up to 1024 bytes ...
	uint8_t s_padding[660]; // Padding to complete 1024 bytes
} ext4_superblock_t;

typedef struct {
	device_t base;
	block_device_t *bdev; // parent (partition)
	ext4_superblock_t sb;
	uint32_t block_group_count;
	void *bg_desc_table;
	// cache: inode cache, block cache, etc
} ext4_fs_t;

// Mount: read superblock, validate magic, create ext4 device node as child of bdev
ext4_fs_t *ext4_mount(block_device_t *bdev);
void ext4_umount(ext4_fs_t *fs);

// Minimal file read API to load driver binaries
int ext_file_read(ext4_fs_t *fs, const char *path, void *dst, size_t offset, size_t len);
