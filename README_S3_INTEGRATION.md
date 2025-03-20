# S3 Integration for SUMO

This document provides information about using SUMO with Amazon S3 storage for Parquet file output.

## Overview

SUMO supports writing simulation outputs directly to Amazon S3 buckets in Parquet format. This feature allows:

- Storing simulation outputs directly in cloud storage
- Integration with data analytics pipelines
- Processing SUMO outputs with big data tools

## Requirements

- SUMO compiled with Parquet and S3 support
- AWS account with appropriate permissions
- AWS credentials (Access Key ID and Secret Access Key)

## Configuration

### AWS Credentials

To authenticate with AWS S3, set the following environment variables:

```bash
# Required
export SUMO_S3_ACCESS_KEY="YOUR_AWS_ACCESS_KEY_ID"
export SUMO_S3_SECRET_KEY="YOUR_AWS_SECRET_ACCESS_KEY"

# Optional
export SUMO_S3_SESSION_TOKEN="YOUR_AWS_SESSION_TOKEN"  # Only for temporary credentials
export SUMO_S3_REGION="us-east-1"  # Defaults to us-east-1 if not specified
export SUMO_S3_ENDPOINT="custom.endpoint.example.com"  # For S3-compatible storage
```

### S3 URL Format

To write to S3, use the `s3://` URL scheme in your configuration:

```
s3://bucket-name/path/to/output.parquet
```

### Environment Variables

Additional environment variables to configure S3 and Parquet behavior:

| Variable | Description | Default |
|----------|-------------|---------|
| SUMO_S3_REGION | AWS region | us-east-1 |
| SUMO_S3_ENDPOINT | Custom endpoint for S3-compatible storage | Based on region |
| SUMO_PARQUET_COMPRESSION | Compression type (ZSTD, SNAPPY, GZIP, NONE) | ZSTD |
| SUMO_PARQUET_ROWGROUP_SIZE | Number of rows per row group | 1,000,000 |
| SUMO_PARQUET_BUFFER_SIZE | Buffer size for schema detection | 10,000 |

## Usage Examples

### Example 1: Writing FCD output to S3

```xml
<additional>
    <fcd-output value="s3://sumo-traffic-bucket/outputs/fcd_output.parquet"/>
</additional>
```
*For now please be explicit to mention fcd in the fcd output filename 
and similarly edge and lane in edge, lane output files* 

### Example 2: Multiple S3 outputs for edge and lane

```xml
<additional>
    <edgeData id="edge_data" file="s3://sumo-traffic-bucket/outputs/edge_data.parquet" freq="300"/>
    <laneData id="lane_data" file="s3://sumo-traffic-bucket/outputs/lane_data.parquet" freq="300"/>
</additional>
```

