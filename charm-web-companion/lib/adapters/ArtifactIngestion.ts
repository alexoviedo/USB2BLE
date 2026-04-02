import { Manifest, ManifestSchema } from '../schema';
import { ArtifactIngestionAdapter } from './types';
import { FlashError } from '../types';

export class BrowserArtifactIngestion implements ArtifactIngestionAdapter {
  async fetchSameSiteManifest(): Promise<Manifest> {
    try {
      const res = await fetch('firmware/manifest.json');
      if (!res.ok) {
        throw new FlashError(`HTTP ${res.status} fetching manifest.json`, 'MISSING_ARTIFACTS');
      }
      const json = await res.json();
      return ManifestSchema.parse(json);
    } catch (err: any) {
      if (err.name === 'ZodError') {
        throw new FlashError('Malformed manifest.json', 'MALFORMED_MANIFEST');
      }
      throw new FlashError(err.message || 'Failed to fetch manifest', 'MISSING_ARTIFACTS');
    }
  }

  async fetchSameSiteBinary(filename: string): Promise<ArrayBuffer> {
    try {
      const res = await fetch(`firmware/${filename}`);
      if (!res.ok) {
        throw new FlashError(`HTTP ${res.status} fetching ${filename}`, 'MISSING_ARTIFACTS');
      }
      return await res.arrayBuffer();
    } catch (err: any) {
      throw new FlashError(`Failed to fetch ${filename}: ${err.message}`, 'MISSING_ARTIFACTS');
    }
  }

  async parseManualManifest(file: File): Promise<Manifest> {
    try {
      const text = await file.text();
      const json = JSON.parse(text);
      return ManifestSchema.parse(json);
    } catch (err: any) {
      if (err.name === 'ZodError' || err instanceof SyntaxError) {
        throw new FlashError('Malformed manual manifest.json', 'MALFORMED_MANIFEST');
      }
      throw new FlashError('Failed to read manual manifest', 'MISSING_ARTIFACTS');
    }
  }

  async readManualBinary(file: File): Promise<ArrayBuffer> {
    try {
      return await file.arrayBuffer();
    } catch (err: any) {
      throw new FlashError(`Failed to read manual binary ${file.name}`, 'MISSING_ARTIFACTS');
    }
  }
}
